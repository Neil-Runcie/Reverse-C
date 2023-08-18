
/*********************************************************************************
*                                    NDR Reverse C                               *
**********************************************************************************/

/*
BSD 3-Clause License

Copyright (c) 2023, Neil Runcie

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NDR_LAP/include/ndr_lap.h"


int numVariables = 0;

typedef struct VariableTracker{
    char varName[10];
    size_t originalLocation;
    size_t memoryNeeded;
} VariableTracker;

VariableTracker varTable[256];

int numLabels = 0;
char labelTable[256][10];

// The codeStrings variable hold references to the 4 strings that will be concatenated to form the final text of the translated program
// 0 for includeString, 1 for headString, 2 for bodyString, 3 for footString
typedef enum printStrings{
    INCLUDESTRING = 0, HEADSTRING = 1, BODYSTRING = 2, FOOTSTRING = 3
} printStrings;

typedef struct CodeComponents{
    size_t ComponentMemory[4];
    char* codeStrings[4];
} CodeComponents;

CodeComponents components;

static int Compile();
static void SetupCHeader();
static void SetupCFooter();
static NDR_ASTNode* GetNextByte();
static int HandleByte(NDR_ASTNode* node);
static int SetValue(NDR_ASTNode* node);
static int MoveValue(NDR_ASTNode* node);
static int SetValueWithVar(NDR_ASTNode* node);
static int CheckEqualityWithVariables(NDR_ASTNode* node);
static int CheckGreaterThanWithVariables(NDR_ASTNode* node);
static int CheckLesserThanWithVariables(NDR_ASTNode* node);
static int EqualThenJumpWithVariables(NDR_ASTNode* node);
static int GreaterThenJumpWithVariables(NDR_ASTNode* node);
static int LesserThenJumpWithVariables(NDR_ASTNode* node);
static int AddWithVariables(NDR_ASTNode* node);
static int SubtractWithVariables(NDR_ASTNode* node);
static int AndWithVariables(NDR_ASTNode* node);
static int OrWithVariables(NDR_ASTNode* node);
static int SetLabel(NDR_ASTNode* node);
static int JumpToLabel(NDR_ASTNode* node);
static int PrintNumber(NDR_ASTNode* node);
static int PrintCharacter(NDR_ASTNode* node);
static int FindInVarTable(char* varName);
static int FindInLabelTable(char* labelName);
static void AllocateVariableMemory();
static void Init_CodeComponents();
static void AddTextToComponent(printStrings stringNumber, char* code);
static void PrintComponentsToFile(FILE* codeFile);



// main calls the lexer, parser, and compiler functions to complete the translation process
int main(int argc, char* argv[])
{
    NDR_Set_Toggles(argc, argv);

    if(NDR_Configure_Lexer("reverse-c.lex") != 0)
        return 1;

    if(NDR_Configure_Parser("reverse-c.par") != 0)
        return 2;

    if(NDR_Lex(argv[1]) != 0)
        return 3;

    if(NDR_Parse() != 0)
        return 4;

    if(Compile() != 0)
        return 5;

    NDR_DestroyASTTree(NDR_ASThead);

    return 0;
}

// Compile handles all translation
int Compile(){
    // Opening the C code file in write mode
    FILE *codeFile;
    codeFile = fopen("CFile.c", "w+");

    // Initializing the strings that will be copied into the C code file
    Init_CodeComponents(components);

    SetupCHeader(codeFile);

    NDR_ASTNode* node = NDR_ASTPostOrderTraversal(NDR_ASThead);
    while(node != NULL){

        if(strcmp(node->keyword, "byte") == 0){
            if(HandleByte(node) != 0){
                fclose(codeFile);
                return -1;
            }
        }
        node = NDR_ASTPostOrderTraversal(NULL);
    }

    AllocateVariableMemory();

    SetupCFooter();

    PrintComponentsToFile(codeFile);

    fclose(codeFile);

    return 0;
}

void SetupCHeader(){
    AddTextToComponent(INCLUDESTRING, "#include <stdio.h>\n#include <stdlib.h>\n\n");
    AddTextToComponent(INCLUDESTRING, "int main(int argc, char* argv[]){\n");
}

void SetupCFooter(){
    for(int x = 0; x < numVariables; x++){
        char holder[44];
        strcpy(holder, "r");
        strcat(holder, varTable[x].varName);
        strcat(holder, " = m");
        strcat(holder, varTable[x].varName);
        strcat(holder, ";\n");
        strcat(holder, "free(r");
        strcat(holder, varTable[x].varName);
        strcat(holder, ");\n");

        AddTextToComponent(FOOTSTRING, holder);
    }
    AddTextToComponent(FOOTSTRING, "}\n");
}

NDR_ASTNode* GetNextByte(){
    NDR_ASTNode* operand = NDR_ASTPostOrderTraversal(NULL);
    while(operand != NULL && strcmp(operand->keyword, "byte") != 0){
        operand = NDR_ASTPostOrderTraversal(NULL);
    }
    return operand;
}

int HandleByte(NDR_ASTNode* node){

    if(strcmp(node->token, "00000001") == 0){
        if(SetValue(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00000010") == 0){
        if(MoveValue(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00000011") == 0){
        if(SetValueWithVar(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00000100") == 0){
        if(CheckEqualityWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00000101") == 0){
        if(CheckGreaterThanWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00000110") == 0){
        if(CheckLesserThanWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00000111") == 0){
        if(EqualThenJumpWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001000") == 0){
        if(GreaterThenJumpWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001001") == 0){
        if(LesserThenJumpWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001010") == 0){
        if(AddWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001011") == 0){
        if(SubtractWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001100") == 0){
        if(AndWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001101") == 0){
        if(OrWithVariables(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001110") == 0){
        if(SetLabel(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00001111") == 0){
        if(JumpToLabel(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00010000") == 0){
        if(PrintNumber(node) != 0){
            return -1;
        }
    }
    else if(strcmp(node->token, "00010001") == 0){
        if(PrintCharacter(node) != 0){
            return -1;
        }
    }
    else{
        printf("Instruction \"%s\" not found", node->token);
        return -1;
    }

    return 0;
}

int SetValue(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000001\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    char holder[42];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000001\"");
        return -1;
    }

    char *error = NULL;
    int number = strtol(operand->token, &error, 2);
    char numToString[10];
    sprintf(numToString, "%i", number);

    strcat(holder, numToString);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int MoveValue(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000010\"");
        return -1;
    }

    int variableEntry = FindInVarTable(operand->token);
    if(variableEntry == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[53];
    strcpy(holder, "r");
    strcat(holder, operand->token);
    strcat(holder, " = ");
    strcat(holder, "r");
    strcat(holder, operand->token);
    strcat(holder, " + sizeof(int) * ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000010\"");
        return -1;
    }

    char *error = NULL;
    int number = strtol(operand->token, &error, 2);
    varTable[variableEntry].memoryNeeded = varTable[variableEntry].memoryNeeded + number;
    char numToString[10];
    sprintf(numToString, "%i", number);

    strcat(holder, numToString);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int SetValueWithVar(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000011\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[28];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000011\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int CheckEqualityWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000100\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[44];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000100\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000100\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, " == *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int CheckGreaterThanWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000101\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[43];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000101\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000101\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, " > *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int CheckLesserThanWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000110\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[43];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000110\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000110\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, " < *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int EqualThenJumpWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000111\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[52];
    strcpy(holder, "if(*r");
    strcat(holder, operand->token);
    strcat(holder, " == ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000111\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, ")\n\tgoto l");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00000111\"");
        return -1;
    }

    if(FindInLabelTable(operand->token) == -1){
        printf("Could not find label \"%s\" to jump to for instruction \"00000111\" on line %u, column %u", operand->token, operand->lineNumber, operand->columnNumber);
        return -1;
    }

    strcat(holder, operand->token);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int GreaterThenJumpWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001000\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[52];
    strcpy(holder, "if(*r");
    strcat(holder, operand->token);
    strcat(holder, " > ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001000\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, ")\n\tgoto l");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001000\"");
        return -1;
    }

    if(FindInLabelTable(operand->token) == -1){
        printf("Could not find label \"%s\" to jump to for instruction \"00001000\" on line %u, column %u", operand->token, operand->lineNumber, operand->columnNumber);
        return -1;
    }

    strcat(holder, operand->token);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int LesserThenJumpWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001001\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[52];
    strcpy(holder, "if(*r");
    strcat(holder, operand->token);
    strcat(holder, " < ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001001\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, ")\n\tgoto l");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001001\"");
        return -1;
    }

    if(FindInLabelTable(operand->token) == -1){
        printf("Could not find label \"%s\" to jump to for instruction \"00001001\" on line %u, column %u", operand->token, operand->lineNumber, operand->columnNumber);
        return -1;
    }

    strcat(holder, operand->token);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int AddWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001010\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[43];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001010\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001010\"");
        return -1;
    }

    strcat(holder, " + *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int SubtractWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001011\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[43];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001011\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001011\"");
        return -1;
    }

    strcat(holder, " - *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int AndWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001100\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[43];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001100\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001100\"");
        return -1;
    }

    strcat(holder, " & *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int OrWithVariables(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001101\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }


    char holder[43];
    strcpy(holder, "*r");
    strcat(holder, operand->token);
    strcat(holder, " = ");

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001101\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    strcat(holder, "(*r");
    strcat(holder, operand->token);

    operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001101\"");
        return -1;
    }

    strcat(holder, " | *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int SetLabel(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001110\"");
        return -1;
    }

    if(FindInLabelTable(operand->token) != -1){
        printf("The label %s already exists", operand->token);
        return -1;
    }
    else{
        strcpy(labelTable[numLabels++], operand->token);
    }


    char holder[14];
    strcpy(holder, "l");
    strcat(holder, operand->token);
    strcat(holder, ":\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}


int JumpToLabel(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00001111\"");
        return -1;
    }

    if(FindInLabelTable(operand->token) == -1){
        printf("The label %s has not been created for use", operand->token);
        return -1;
    }


    char holder[19];
    strcpy(holder, "goto l");
    strcat(holder, operand->token);
    strcat(holder, ";\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int PrintNumber(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00010000\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    char holder[31];
    strcpy(holder, "printf(\"%i\", *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);


    return 0;
}

int PrintCharacter(NDR_ASTNode* node){

    NDR_ASTNode* operand = GetNextByte();
    if(operand == NULL){
        printf("Program ended before expected number of operands were found for instruction \"00010001\"");
        return -1;
    }

    if(FindInVarTable(operand->token) == -1){
        strcpy(varTable[numVariables].varName, operand->token);
        varTable[numVariables++].memoryNeeded = 1;
    }

    char holder[31];
    strcpy(holder, "printf(\"%c\", *r");
    strcat(holder, operand->token);
    strcat(holder, ");\n");

    AddTextToComponent(BODYSTRING, holder);

    return 0;
}

int FindInVarTable(char* varName){
    for(int x = 0; x < numVariables; x++){
        if(strcmp(varName, varTable[x].varName) == 0){
            return x;
        }
    }
    return -1;
}

int FindInLabelTable(char* labelName){
    for(int x = 0; x < numLabels; x++){
        if(strcmp(labelName, labelTable[x]) == 0){
            return x;
        }
    }
    return -1;
}

void AllocateVariableMemory(){
    for(int x = 0; x < numVariables; x++){

        char holder[70];
        //char* holder = malloc(strlen("int* r00000000 = malloc(sizeof30(int) * 256);\n "));
        // 21 is chosen because the largest number that can be held in size_t is 20 digits long + null terminator
        char memoryAmount[21];
        strcpy(holder, "int* r");
        strcat(holder, varTable[x].varName);
        strcat(holder, " = ");
        strcat(holder, "malloc(sizeof(int) * ");
        sprintf(memoryAmount, "%u", varTable[x].memoryNeeded);
        strcat(holder, memoryAmount);
        strcat(holder, ");\n");

        AddTextToComponent(HEADSTRING, holder);

        char holder2[31];
        strcpy(holder2, "int* m");
        strcat(holder2, varTable[x].varName);
        strcat(holder2, " = ");
        strcat(holder2, "r");
        strcat(holder2, varTable[x].varName);
        strcat(holder2, ";\n");

        AddTextToComponent(HEADSTRING, holder2);

    }
}

void Init_CodeComponents(){
    components.ComponentMemory[INCLUDESTRING] = 100;
    components.ComponentMemory[HEADSTRING] = 100;
    components.ComponentMemory[BODYSTRING] = 100;
    components.ComponentMemory[FOOTSTRING] = 100;

    components.codeStrings[INCLUDESTRING] = malloc(components.ComponentMemory[INCLUDESTRING]);
    components.codeStrings[HEADSTRING] = malloc(components.ComponentMemory[HEADSTRING]);
    components.codeStrings[BODYSTRING] = malloc(components.ComponentMemory[BODYSTRING]);
    components.codeStrings[FOOTSTRING] = malloc(components.ComponentMemory[FOOTSTRING]);

    strcpy(components.codeStrings[INCLUDESTRING], "");
    strcpy(components.codeStrings[HEADSTRING], "");
    strcpy(components.codeStrings[BODYSTRING], "");
    strcpy(components.codeStrings[FOOTSTRING], "");
}

void AddTextToComponent(printStrings stringNumber, char* code){
    if(strlen(components.codeStrings[stringNumber]) + strlen(code) >= components.ComponentMemory[stringNumber] - 5){
        components.ComponentMemory[stringNumber] *= 2;
        components.codeStrings[stringNumber] = realloc(components.codeStrings[stringNumber], components.ComponentMemory[stringNumber]);
    }

    strcat(components.codeStrings[stringNumber], code);
}

void PrintComponentsToFile(FILE* codeFile){
    fprintf(codeFile, "%s\n", components.codeStrings[INCLUDESTRING]);
    fprintf(codeFile, "%s\n", components.codeStrings[HEADSTRING]);
    fprintf(codeFile, "%s\n", components.codeStrings[BODYSTRING]);
    fprintf(codeFile, "%s\n", components.codeStrings[FOOTSTRING]);
}
