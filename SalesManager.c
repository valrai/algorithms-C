#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <libpq-fe.h>

#define STOCK_MAX_NUMBER_OF_RECORDS  100
#define MAX_PRODUCT_NAME_LENGTH  200
#define MAX_PRODUCT_CODE_LENGTH  100
#define MAX_CONNECTION_STRING_LENGTH  100

const float PROFIT_PERCENTAGE = 0.2;
typedef enum {  Ok = 200, BadRequest = 400, NotFound = 404, ServerError = 500, InsufficientStorage = 507} ResultStatus;
typedef enum { Purchase = 1, Sale = 2 } StockMovimentationType;

typedef struct
{
    int id;
    char* code;
    char* name;
    float costPrice;
    float sellingPrice;
    int quantity;

}Product;   

typedef struct
{
    int id;     
    int saleId;
    int purchaseId;
    time_t date;
    StockMovimentationType type;
    int previousQuant;
    int quant;
    double totalCost;

}StockMovimentation;

typedef struct StockMovimentation_Product
{
    int id;
    StockMovimentation stockMovimentation;
    Product product;
    int productQuantity;
    float productUnitaryPrice;

}SM_Product;

typedef struct 
{
    Product products[STOCK_MAX_NUMBER_OF_RECORDS];
    int lastIndex;

}ProductsList;

typedef struct
{
    char* message;
    ResultStatus status; 

}Result;

//================================================= Helpers ============================================================

void ClearBuffer()
{
	int c;
	while ( (c = getchar()) != '\n' && c != EOF ) { }
}

void RemoveNewLine(char *phase)
{
    int WordSize = strlen(phase);

    if (phase[WordSize - 1] == '\n') 
        phase[WordSize - 1] = '\0';
}

//=====================================================================================================================

void CloseDbConnection(PGconn *conn, PGresult *res) {
    
    char *errorMessage = PQerrorMessage(conn);

    PQclear(res);
    PQfinish(conn);    
    
    if (strlen(errorMessage) > 0)
    {
        fprintf(stderr, "%s\n", errorMessage);    
        exit(1);
    }
}

void SetConnectionString(char * connectionString)
{
    FILE *fp;
    char fileName[] = "DbConnectionString.txt";

    fp = fopen(fileName, "r");

    if (fp == NULL)
    {
      printf("Error while opening the file: %s.\n", fileName);
      exit(EXIT_FAILURE);
    }
    
    fgets(connectionString, MAX_CONNECTION_STRING_LENGTH, fp);
    fclose(fp);

}

PGresult* DbQuery(PGconn *conn, char* query)
{
    return PQexec(conn, query);
}

void PrintAllProducts(PGresult *res)
{
    int nRows = PQntuples(res);
    int nCols = PQnfields(res);
    
    for(int i = 0; i < nRows; i++)
    {
        for (int j = 0; j < nCols; j++)                 
            printf("%s  ", PQgetvalue(res, i, j));
        printf("\n");
    }
}

void PrintProductsList(ProductsList *productsList)
{
    for (int i = 0; i < productsList->lastIndex; i++)
    {
        printf("===================================================\n");
        printf("%s  %s  %2.f  %2.f  %d\n", productsList->products[i].code, productsList->products[i].name, productsList->products[i].costPrice, productsList->products[i].sellingPrice, productsList->products[i].quantity); 
        printf("===================================================\n\n");
    }
}

void SetSellingPrice(Product *product)
{
    float sellingPrice = product->costPrice + (product->costPrice*PROFIT_PERCENTAGE);
    product -> sellingPrice = sellingPrice;
}


Result RegisterProduct(ProductsList *products, PGconn *conn)
{
    Result result;
    Product product;

    char name[MAX_PRODUCT_NAME_LENGTH], code[MAX_PRODUCT_CODE_LENGTH];
    int quantity;
    float costPrice;

    system("clear||cls");
    printf("\n================================================\n\n");
    printf("Inform the code of the product: ");
    fgets(code, MAX_PRODUCT_CODE_LENGTH, stdin);
    RemoveNewLine(code);
    printf("\nInform the name of the product: ");
    fgets(name, MAX_PRODUCT_NAME_LENGTH, stdin);
    RemoveNewLine(name);
    printf("\nInform the quantity of products: ");
    scanf("%d", &quantity);
    printf("\nInform the purchase cost of the product: ");
    scanf("%f", &costPrice);
    printf("\n================================================\n");   

    product.name = name;
    product.code = code;
    product.quantity = quantity;
    product.costPrice = costPrice;
    SetSellingPrice(&product);

    ProductsList pl;
    pl.products[0] = product;
    PrintProductsList(&pl);


    char query[600];
    snprintf(query, 600, "INSERT INTO \"Product\"(code, \"costPrice\", name, \"sellingPrice\", quantity) VALUES (\'%s\' , %.2f, \'%s\', %.2f, %d);", product.code, product.costPrice, product.name, product.sellingPrice, product.quantity);

    PGresult* res = DbQuery(conn, query);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {

        result.message = strcat("Insertion failed, ", PQerrorMessage(conn));
        result.status = ServerError;        
    }
    else{
         result.message = "Product insert sucessfully";
         result.status = Ok;
    }
    PQclear(res);
    return result;
}

void MountProducts(PGresult *res, ProductsList *productsList)
{
    int nRows = PQntuples(res);

    Product product;
    
    for(int i = 0; i < nRows; i++)
    {
        product.code = PQgetvalue(res, i, 0);
        product.costPrice = strtof(PQgetvalue(res, i, 1), NULL);
        product.id = atoll(PQgetvalue(res, i, 2));
        strcpy(product.name, PQgetvalue(res, i, 3));
        product.sellingPrice = strtof(PQgetvalue(res, i, 4), NULL);

        productsList->products[productsList->lastIndex] = product;
        productsList->lastIndex++;
    }
}

int main() 
{    
    char connectionString[MAX_CONNECTION_STRING_LENGTH];    
    SetConnectionString(connectionString);
    ProductsList products;
    products.lastIndex = 0;

    PGconn *conn = PQconnectdb(connectionString);
           
    if (PQstatus(conn) == CONNECTION_BAD) 
    {
        fprintf(stderr, "Connection to database failed: %s\n",
        PQerrorMessage(conn));

        PQfinish(conn);
        exit(1);
    }
   

    // PGresult *res = DbQuery(conn, "SELECT * FROM \"Product\"");

    // if (PQresultStatus(res) != PGRES_TUPLES_OK) {

    //     printf("No data found\n");        
    //     CloseDbConnection(conn, res);
    // } 

    // MountProducts(res, &products);
    // PrintListProducts(&products);

    Result result = RegisterProduct(&products, conn);
    printf("\n================\n%s", result.message);

    PQfinish(conn);

    // CloseDbConnection(conn, res);

    return 0;
}