// File: setcal.c
// Subject: IZP
// Project: #2
// Author: Andrii Klymenko, FIT VUT
// Login: xklyme00
// Date: 28.7.2023

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_LINE_NUMBER 1000

#define MAX_LINE_LENGTH MAX_LINE_NUMBER
#define LINE_BUFFER_SIZE MAX_LINE_LENGTH + 2 // + '\n' + '\0'

#define MAX_SET_ELEMENT_LENGTH 30

#define DELIMITER_STR " " // for strtok() function
#define DELIMITER_CHAR ' '

#define MAX_OPERATOR_LENGTH 13 // strlen("closure_trans") == 13
#define OPERATOR_BUFFER_SIZE MAX_OPERATOR_LENGTH + 1 // + '\0'

#define MAX_OPERANDS_NUMBER 4 // command can have maximum 4 operands (for example: "injective  id1 id2 id3 go_to_line)"

// commands over sets
const char *set_commands[] = {
        "empty",
        "card",
        "complement",
        "union",
        "intersect",
        "minus",
        "subseteq",
        "subset",
        "equals"
};

// commands over relations + "select" which works with both sets and relations
const char *relation_commands[] = {
        "reflexive",
        "symmetric",
        "antisymmetric",
        "transitive",
        "function",
        "domain",
        "codomain",
        "injective",
        "surjective",
        "bijective",
        "closure_ref",
        "closure_sym",
        "closure_trans",
        "select"
};

// outputs of some commands over sets/relations
const char *key_words[] = {
        "false",
        "true"
};

// structure represents a command over sets/relations (or both in case of "select")
typedef struct {
    char name[OPERATOR_BUFFER_SIZE]; // name of the command over sets/relations
    int operands[MAX_OPERANDS_NUMBER]; // array of integers for keeping command operands
} Command_t;

// structure represents an element of the set
typedef struct {
    char *name; // name of the set element
} Set_element_t;

// structure represents both set and relation element
// we can say that set element is a relation pair where element2.name == NULL
typedef struct {
    Set_element_t element1; // first set element (x)

    // second set element (y). in the case when relation represent a set element it is NULL(element2.name == NULL)
    Set_element_t element2;
} Relation_pair_t; // if it represents a relation pair => it is (x, y). if it represents a set element => it is (x)

// structure represents a set/relation
typedef struct {
    int size; // number of relation pairs/set elements in relation/set
    int id; // id of relation/set

    // specifies if relation/set is sorted by element1.name (by its first value, x) or not (by element2.name, by y)
    bool sortedByX;
    Relation_pair_t *pair_arr; // array of relation pairs/set elements
} Relation_t;

// structure represents a set/relation array
typedef struct {
    int size; // number of relations/sets in the array
    Relation_t *relation_arr; // array of relations/sets
} Relation_arr_t;

// prints program usage
void printUsage()
{
    fprintf(stderr, "Usage: ./setcal FILE\n");
}

// assigns a name to the set element
void *setElementCtor(Set_element_t *e, char *name)
{
    if(name == NULL) // if there is no name
    {
        e->name = NULL;
        return e;
    }

    e->name = (char *) malloc(strlen(name) + 1);

    if(e->name == NULL) // malloc failed
        return NULL;

    strcpy(e->name, name);
    return e;
}

// assigns a name1 and name2 to the relation pair/set element
void *relationPairCtor(Relation_pair_t *p, char *name1, char *name2)
{
    if(setElementCtor(&p->element1, name1) == NULL)
        return NULL;

    return setElementCtor(&p->element2, name2);
}

// frees the memory allocated for a set element
void setElementDtor(Set_element_t *e)
{
    if(e->name != NULL) // free the memory only if it was previously allocated (i.e. it is not NULL)
    {
        free(e->name);
        e->name = NULL; // set a pointer to NULL
    }
}

// frees the memory allocated for a relation pair/set element
void relationPairDtor(Relation_pair_t *p)
{
    setElementDtor(&p->element1);
    setElementDtor(&p->element2);
}

// initializes a relation/set
void relationCtor(Relation_t *r, int id)
{
    r->id = id;
    r->size = 0;
    r->pair_arr = NULL;
    r->sortedByX = false;
}

// frees the memory allocated for a relation/set
void relationDtor(Relation_t *r)
{
    if(r != NULL)
    {
        for(int i = 0; i < r->size; i++)
            relationPairDtor(&r->pair_arr[i]); // free every single relation pair/set element of the relation/set

        free(r->pair_arr); // free an array of relation pairs/set elements
        r->pair_arr = NULL; // set a pointer to NULL
    }

    r->size = 0;
    r->id = 0;
}

// initializes an array of relations/sets
void relationArrayCtor(Relation_arr_t *a)
{
    a->size = 0;
    a->relation_arr = NULL;
}

// frees the memory allocated for a relation/set array
void relationArrayDtor(Relation_arr_t *a)
{
    if(a != NULL)
    {
        for(int i = 0; i < a->size; i++)
            relationDtor(&a->relation_arr[i]); // free every single relation/set of the relation/set array

        free(a->relation_arr); // free an array of relations/sets
        a->relation_arr = NULL; // set a pointer to NULL
    }

    a->size = 0;
}

// resizes a relation/set
void *relationResize(Relation_t *r, int new_size)
{
    // allocate a new block of memory with a desired size
    Relation_pair_t *tmp = (Relation_pair_t *) realloc(r->pair_arr, new_size * sizeof(Relation_pair_t));

    if(tmp == NULL) // realloc failed
    {
        relationDtor(r); // frees an original block of memory
        return NULL;
    }

    r->pair_arr = tmp; // assign an allocated block of memory

    for(int i = r->size; i < new_size; i++)
        relationPairCtor(&r->pair_arr[i], NULL, NULL); // initialize all new pairs

    r->size = new_size; // set a new size

    return r;
}

// resizes a relation/set array
void *relationArrayResize(Relation_arr_t *a, int new_size)
{
    // allocate a new block of memory with a desired size
    Relation_t *tmp = (Relation_t *) realloc(a->relation_arr, new_size * sizeof(Relation_t));

    if(tmp == NULL) // realloc failed
    {
        relationArrayDtor(a); // frees an original block of memory
        return NULL;
    }

    a->size = new_size; // set a new size
    a->relation_arr = tmp; // assign an allocated block of memory

    return a;
}

// copies a relation/set
void *relationCopy(Relation_t *dst, Relation_t *src)
{
    // resize a destination set/relation
    if(relationResize(dst, src->size) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize a set/relation\n");
        return NULL;
    }

    // 'copy' all relations pairs/set elements from src to dst
    for(int i = 0; i < dst->size; i++)
    {
        if(relationPairCtor(&dst->pair_arr[i], src->pair_arr[i].element1.name, src->pair_arr[i].element2.name) == NULL)
        {
            fprintf(stderr, "Error! Couldn't initialize a set element\n");
            return NULL;
        }
    }

    return dst;
}

// prints a set
void printSet(Relation_t *s)
{
    if(s->id != 1)
        putchar('S');
    else
        putchar('U');

    for(int i = 0; i < s->size; i++)
        printf(" %s", s->pair_arr[i].element1.name);

    putchar('\n');
}

// prints a relation
void printRelation(Relation_t *r)
{
    putchar('R');

    for(int i = 0; i < r->size; i++)
        printf(" (%s %s)", r->pair_arr[i].element1.name, r->pair_arr[i].element2.name);

    putchar('\n');
}

//void printSetArray(Relation_arr_t *a)
//{
//    for(int i = 0; i < a->size; i++)
//        printSet(&a->relation_arr[i]);
//
//}
////
//void printRelationArray(Relation_arr_t *a)
//{
//    for(int i = 0; i < a->size; i++)
//        printRelation(&a->relation_arr[i]);
//}

// checks a length of the line
bool checkLineLength(char *line)
{
    if(strchr(line, '\n') == NULL) // check if line isn't too long (i.e. is has a '\n' character)
        return false;

    // replace '\n' with '\0' because we don't need it
    line[strcspn(line, "\n")] = '\0';

    int line_length = strlen(line);

    // the line of the file must not be empty
    // if line contains a command over sets/relations its length must be greater or equal to 8
    // because strlen("C card 1") == 8 and card is 'the shortest' command
    return line_length != 0 && (*line != 'C' || line_length >= 8);
}

// checks if line doesn't contain a two following each other delimiters (spaces)
bool checkDelimiterSequence(char *line)
{
    for(int i = 0; line[i] != '\0'; i++)
        if(line[i] == DELIMITER_CHAR && line[i + 1] == DELIMITER_CHAR)
            return false;

    return true;
}

// checks the right order of the lines in the file
bool isValidSequence(char current_line, char *last_line, int line_cnt)
{
    if(line_cnt == 0) // first line must 'declare' a universal set (i.e. must start from the character 'U')
    {
        if(current_line == 'U')
        {
            *last_line = current_line; // writes a first character of the line to the *last_line
            return true;
        }

        return false;
    }

    // right order of the lines in the file: U -> R/S -> C
    switch (*last_line)
    {
        case 'U':
            if(current_line != 'R' && current_line != 'S')
                return false;
            break;

        case 'R':
        case 'S':
            if(current_line != 'R' && current_line != 'S' && current_line != 'C')
                return false;
            break;

        case 'C':
            if(current_line != 'C')
                return false;
            break;
    }

    *last_line = current_line; // writes a first character of the line to the *last_line
    return true;
}

// checks if a line from the file is valid
bool isValidLine(char *line, int line_cnt, char *last_line)
{
    if(!checkLineLength(line))
    {
        fprintf(stderr, "Error! Invalid line no. %d length\n", line_cnt + 1);
        return false;
    }

    // check if line doesn't contain a delimiter (space) as its last character
    if(line[strcspn(line, "\0") - 1] == DELIMITER_CHAR)
    {
        fprintf(stderr, "Error! Delimiter (space) must not be the last character of the line (line no. %d)\n", line_cnt + 1);
        return false;
    }

    if(!isValidSequence(line[0], last_line, line_cnt))
    {
        fprintf(stderr, "Error! Invalid file format: it must be like that: U -> R/S -> C (line no. %d)\n", line_cnt + 1);
        return false;
    }

    if(line[1] == '\0') // in the case when a set/relation is empty
        return true;

    // if a set/relation is not empty there should be a delimiter (space) as a second character of the line
    if(line[1] != DELIMITER_CHAR)
    {
        fprintf(stderr, "Error! If line doesn't declare an empty set/relation,"
                        "there should be a space (delimiter) as a second character (line no. %d)\n", line_cnt + 1);
        return false;
    }

    if(checkDelimiterSequence(line))
        return true;

    fprintf(stderr, "Error! Line no. %d contains two or more delimiters (spaces) following each other\n", line_cnt + 1);
    return false;
}

// checks if each character from the string is an alphabetic
bool isOnlyAlpha(char *str)
{
    for(int i = 0; str[i] != '\0'; i++)
        if(!isalpha((int) str[i]))
            return false;

    return true;
}

// checks if string contains a command over sets/relations
bool isCommand(char *str)
{
    for(int i = 0; i < 9; i++)
        if(strcmp(str, set_commands[i]) == 0 || strcmp(str, relation_commands[i]) == 0)
            return true;

    for(int i = 9; i < 14; i++)
        if(strcmp(str, relation_commands[i]) == 0)
            return true;

    return false;
}

// checks if string contains a keyword "true" or "false"
bool isKeyWord(char *str)
{
    return strcmp(str, key_words[0]) == 0 || strcmp(str, key_words[1]) == 0;
}

// checks if string contains a valid set element
bool isValidSetElement(char *str)
{
    return strlen(str) <= MAX_SET_ELEMENT_LENGTH && isOnlyAlpha(str) && !isCommand(str) &&
           !isKeyWord(str);
}

// returns number of delimiters (spaces) in the string
int numberOfDelimiters(char *str)
{
    int cnt = 0;
    for(int i = 0; str[i] != '\0'; i++)
        if(str[i] == DELIMITER_CHAR)
            cnt++;

    return cnt;
}

// swaps two set elements
void swapElements(Set_element_t *e1, Set_element_t *e2)
{
    Set_element_t tmp = *e1;
    *e1 = *e2;
    *e2 = tmp;
}

// sorts a set/relation by its first element (element1.name, in other words by x) using a bubble sort algorithm
void sortRelationByX(Relation_t *r)
{
    for(int i = 0; i < r->size - 1; i++)
    {
        for(int j = 0; j < r->size - 1 - i; j++)
        {
            if(strcmp(r->pair_arr[j].element1.name, r->pair_arr[j + 1].element1.name) > 0)
            {
                swapElements(&r->pair_arr[j].element1, &r->pair_arr[j + 1].element1);

                // if relation is sorted (i.e. its element2.name != NULL) also swap its second elements
                if(r->pair_arr[j].element2.name != NULL)
                    swapElements(&r->pair_arr[j].element2, &r->pair_arr[j + 1].element2);
            }
        }
    }

    r->sortedByX = true;
}

// sorts a relation by its second element (element2.name, in other words by y) using a bubble sort algorithm
void sortRelationByY(Relation_t *r)
{
    for(int i = 0; i < r->size - 1; i++)
    {
        for(int j = 0; j < r->size - 1 - i; j++)
        {
            if(strcmp(r->pair_arr[j].element2.name, r->pair_arr[j + 1].element2.name) > 0)
            {
                swapElements(&r->pair_arr[j].element1, &r->pair_arr[j + 1].element1);
                swapElements(&r->pair_arr[j].element2, &r->pair_arr[j + 1].element2);
            }
        }
    }

    r->sortedByX = false;
}

// check if a set/relation contains a duplicate set elements/relation pairs
bool containDuplicate(Relation_t *r)
{
    for(int i = 0; i < r->size - 1; i++)
    {
        if(strcmp(r->pair_arr[i].element1.name, r->pair_arr[i + 1].element1.name) == 0 &&
           (r->pair_arr[i].element2.name == NULL || strcmp(r->pair_arr[i].element2.name, r->pair_arr[i + 1].element2.name) == 0))
        {
            return true;
        }
    }

    return false;
}

// checks if the element is in the set using binary search algorithm
bool isInSet(Relation_t *s, Set_element_t *e)
{
    if(s->size == 0) // if set is empty
        return false;

    int l = 0;
    int h = s->size - 1;
    int m;

    while(l <= h)
    {
        m = (l + h) / 2;

        // compares only first element of a set/relation pair
        int compare = strcmp(e->name, s->pair_arr[m].element1.name);

        if(compare > 0)
            l = m + 1;
        else if(compare < 0)
            h = m - 1;
        else
            return true;
    }

    return false;
}

// parses a set from the file
bool parseSet(char *line, Relation_arr_t *set_arr, int line_cnt)
{
    // resizes an array of sets in order to be able to store one more set, i.e. increment its size
    if(relationArrayResize(set_arr, set_arr->size + 1) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize an array of set_arr\n");
        return false;
    }

    // initialize a new set on the freed memory block
    relationCtor(&set_arr->relation_arr[set_arr->size - 1], line_cnt + 1);

    if(line[1] == '\0') // if set is empty
    {
        printSet(&set_arr->relation_arr[set_arr->size - 1]);
        return true;
    }

    // if line declaring a set has a right format (S element1 element2 ...)
    // there are a 'numberOfDelimiters(line)' set elements
    // so resize a new set with size 'numberOfDelimiters(line)'
    if(relationResize(&set_arr->relation_arr[set_arr->size - 1], numberOfDelimiters(line)) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize a set\n");
        return false;
    }

    char *token = strtok(line + 2, DELIMITER_STR); // get a first set element
    int i = 0;

    while(token != NULL) // while not all set elements were processed
    {
        if(!isValidSetElement(token))
        {
            fprintf(stderr, "Error! Invalid set element '%s'\n", token);
            return false;
        }

        // initialize a set element
        if(relationPairCtor(&set_arr->relation_arr[set_arr->size - 1].pair_arr[i], token, NULL) == NULL)
        {
            fprintf(stderr, "Error! Couldn't allocate memory for a set element\n");
            return false;
        }

        if(line_cnt != 0)
        {
            // check if a set element belongs to the universal set
            if(!isInSet(set_arr->relation_arr, &set_arr->relation_arr[set_arr->size - 1].pair_arr[i].element1))
            {
                fprintf(stderr, "Error! Each set element must belong to the universal set\n");
                return false;
            }
        }

        token = strtok(NULL, DELIMITER_STR); // get next set element
        i++;
    }

    sortRelationByX(&set_arr->relation_arr[set_arr->size - 1]); // sort a set

    // check if set contains a duplicate elements or not
    if(containDuplicate(&set_arr->relation_arr[set_arr->size - 1]))
    {
        fprintf(stderr, "Error! Each set element must be unique\n");
        return false;
    }

    printSet(&set_arr->relation_arr[set_arr->size - 1]); // print set
    return true;
}

// checks if set/relation is empty or not
bool isEmpty(Relation_t *r)
{
    return r->size == 0;
}

// prints a size of the set ("card" command)
void printSetSize(Relation_t *r)
{
    printf("%d\n", r->size);
}

// finds and returns a set/relation id in set/relation array by id using binary search algorithm
// if it couldn't find a set/relation with specified id returns NULL
Relation_t *findById(Relation_arr_t *a, int id)
{
    int l = 0;
    int h = a->size - 1;
    int m;

    while(l <= h)
    {
        m = (l + h) / 2;

        int compare = id - a->relation_arr[m].id;

        if(compare > 0)
            l = m + 1;
        else if(compare < 0)
            h = m - 1;
        else
            return &a->relation_arr[m];
    }

    return NULL;
}

// checks if command over sets/relations has two mandatory parameters or not
bool isBinaryCommand(char *operation)
{
    for(int i = 3; i < 9; i++)
        if(strcmp(operation, set_commands[i]) == 0)
            return true;

    return strcmp(operation, relation_commands[13]) == 0;
}

// checks if command over sets/relations has three mandatory parameters or not
bool isTernaryOperation(char *operation)
{
    for(int i = 7; i < 10; i++)
        if(strcmp(operation, relation_commands[i]) == 0)
            return true;

    return false;
}

// checks if string contains a valid command parameter
// i.e. it must contain an integer from the interval [1, 999] inclusively
bool checkCommandParam(char *str, int *id_number)
{
    if(str == NULL)
        return false;

    char *end_ptr;
    *id_number = (int) strtol(str, &end_ptr, 10); // assigns an id to the *id_number

    return *end_ptr == '\0' && *id_number >= 1 && *id_number <= 999;
}

// checks if command over sets/relations prints "true" or "false" as its output
bool printsTrueOrFalse(char *operation)
{
    if(strcmp(operation, set_commands[0]) == 0)
        return true;

    for(int i = 6; i <= 8; i++)
    {
        if(strcmp(operation, set_commands[i]) == 0)
            return true;
    }

    for(int i = 0; i <= 4; i++)
    {
        if(strcmp(operation, relation_commands[i]) == 0)
            return true;
    }

    for(int i = 7; i <= 9; i++)
    {
        if(strcmp(operation, relation_commands[i]) == 0)
            return true;
    }

    return false;
}

// parses an argument that represents a number of the line from the file
// from which the program must continue reading the file
// in the case when the output of the command over sets/relations is "false"
bool parseSkipArgument(char **token, int *go_to_line)
{
    if(*token == NULL) // if there isn't an argument 'go_to_line'
        return true;

    if(!checkCommandParam(*token, go_to_line)) // write the argument to *go_to_line
        return false;

    *token = strtok(NULL, DELIMITER_STR); // rubbish value (it should be NULL)

    return *token == NULL;
}

// parses a command over sets/relations
bool parseOperation(char *line, Command_t *c)
{
    char *token = strtok(line + 2, DELIMITER_STR); // command name

    if(token == NULL || !isCommand(token)) // if there is no command name or if token doesn't contain a valid command
        return false;

    strcpy(c->name, token); // copy command name

    token = strtok(NULL, DELIMITER_STR); // first command parameter

    if(!checkCommandParam(token, &c->operands[0])) // write first command parameter to c->operands[0] if it is valid
        return false;

    token = strtok(NULL, DELIMITER_STR); // second command parameter

    bool isBinary = isBinaryCommand(c->name);
    bool isTernary = isTernaryOperation(c->name);

    if(!isBinary && !isTernary) // if command has only one mandatory parameter
    {
        if(printsTrueOrFalse(c->name))
        {
            // write go_to_line argument to c->operands[3] if it presents and it is valid
            if(!parseSkipArgument(&token, &c->operands[3]))
                return false;
        }

        return token == NULL; // there must not be any value after first parameter/go_to_line parameter
    }
    else // if command has two or three mandatory parameters
    {
        if(token == NULL) // if there are no parameters more
            return false;
    }

    if(!checkCommandParam(token, &c->operands[1])) // write second command parameter to c->operands[1] if it is valid
        return false;

    if(strcmp(c->name, relation_commands[13]) == 0) // if command is "select"
    {
        // assign go_to_line value to c->operands[3] and assign 0 to c->operands[1] (make s 'swap')
        c->operands[3] = c->operands[1];
        c->operands[1] = 0;
    }

    token = strtok(NULL, DELIMITER_STR); // third command parameter

    if(isBinary) // if command has two mandatory parameters
    {
        if(printsTrueOrFalse(c->name))
        {
            // write go_to_line argument to c->operands[3] if it presents and it is valid
            if(!parseSkipArgument(&token, &c->operands[3]))
                return false;
        }

        return token == NULL; // there must not be any value after second parameter/go_to_line parameter
    }

    // if(isTernary)

    if(!checkCommandParam(token, &c->operands[2])) // write third command parameter to c->operands[2] if it is valid
        return false;

    token = strtok(NULL, DELIMITER_STR); // go_to_line or rubbish value

    if(printsTrueOrFalse(c->name))
    {
        // write go_to_line argument to c->operands[3] if it presents and it is valid
        if(!parseSkipArgument(&token, &c->operands[3]))
            return false;
    }

    return token == NULL; // there must not be any value after third parameter/go_to_line parameter
}

// resizes a relation/set and initializes a relation pair/set element on index 'r->size - 1'
bool resizeAndPairCtor(Relation_t *r, char *element1, char *element2)
{
    if(relationResize(r, r->size + 1) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize a set/relation\n");
        return false;
    }

    if(relationPairCtor(&r->pair_arr[r->size - 1], element1, element2) == NULL)
    {
        fprintf(stderr, "Error! Couldn't initialize a set element/relation pair\n");
        return false;
    }

    return true;
}

// prints difference of two sets (a \ b)
// the difference of two sets will be stored in the set 'new'
bool printDifference(Relation_t *a, Relation_t *b, Relation_t *new)
{
    // firstly sort both sets

    if(!a->sortedByX)
        sortRelationByX(a);

    if(!b->sortedByX)
        sortRelationByX(b);

    for(int i = 0, j = 0; i < a->size; ) // loop until all elements from set 'a' are processed
    {
        int compare;

        if(j != b->size) // if not all elements from the set 'b' were processed
            compare = strcmp(a->pair_arr[i].element1.name, b->pair_arr[j].element1.name);

        // if all elements from the set 'b' were processed or a->pair_arr[i].element1.name is not in the set 'b'
        if(j == b->size || compare < 0)
        {
            if(!resizeAndPairCtor(new, a->pair_arr[i].element1.name, a->pair_arr[i].element2.name))
                return false;

            i++;
        }
        else if(compare > 0)
            j++;
        else
        { // if elements are the same
            i++;
            j++;
        }
    }

    printSet(new);
    return true;
}

// prints union of sets a and b
// the union of two sets will be stored in the set 'new'
bool printUnion(Relation_t *a, Relation_t *b, Relation_t *new)
{
    // firstly sort both sets

    if(!a->sortedByX)
        sortRelationByX(a);

    if(!b->sortedByX)
        sortRelationByX(b);

    // union of one non-empty set and empty set is first non-empty set
    if(isEmpty(a) && !isEmpty(b))
        relationCopy(new,b);
    else if(!isEmpty(a) && isEmpty(b))
        relationCopy(new, a);
    else
    {
        // if both sets are non-empty

        for(int i = 0, j = 0; i < a->size || j < b->size; ) // loop until all elements from both sets are processed
        {
            int compare = 0;

            if(i != a->size && j != b->size) // if not all elements from both sets are processed
                compare = strcmp(a->pair_arr[i].element1.name, b->pair_arr[j].element1.name);

            // if all elements from set 'a' were processed or 'b->pair_arr[j].element1.name' is not in set 'a'
            if(i == a->size || compare > 0)
            {
                if(!resizeAndPairCtor(new, b->pair_arr[j].element1.name, b->pair_arr[j].element2.name))
                    return false;

                j++;
            }
                // if all elements from set 'b' were processed or 'a->pair_arr[i].element1.name' is not in set 'b'
            else if(j == b->size || compare < 0)
            {
                if(!resizeAndPairCtor(new, a->pair_arr[i].element1.name, a->pair_arr[i].element2.name))
                    return false;

                i++;
            }
            else // if compare == 0 => elements are the same
            {
                if(!resizeAndPairCtor(new, a->pair_arr[i].element1.name, a->pair_arr[i].element2.name))
                    return false;

                i++;
                j++;
            }
        }
    }

    printSet(new);
    return true;
}

// prints intersection of sets a and b
// the intersection of two sets will be stored in the set 'new'
bool printIntersection(Relation_t *r1, Relation_t *r2, Relation_t *new)
{
    // firstly sort both sets

    if(!r1->sortedByX)
        sortRelationByX(r1);

    if(!r2->sortedByX)
        sortRelationByX(r2);

    for(int i = 0, j = 0; i < r1->size && j < r2->size; ) // loop until at least one set is fully processed
    {
        int compare = strcmp(r1->pair_arr[i].element1.name, r2->pair_arr[j].element1.name);

        if(compare < 0)
            i++;
        else if(compare > 0)
            j++;
        else
        {
            // if elements are the same

            if(!resizeAndPairCtor(new, r1->pair_arr[i].element1.name, r1->pair_arr[i].element2.name))
                return false;

            i++;
            j++;
        }
    }

    printSet(new);
    return true;
}

// checks if set 'a' is a subset of set 'b'
bool isSubset(Relation_t *a, Relation_t *b)
{
    // if size of set 'a' is greater than a size of set 'b' it can't be a subset of set 'b'
    if(a->size > b->size)
        return false;

    if(isEmpty(a)) // empty set is a subset of every set
        return true;
    else
    {
        if(isEmpty(b)) // non-empty set can't be a subset of empty set
            return false;
    }

    for(int i = 0; i < a->size; i++)
        if(!isInSet(b, &a->pair_arr[i].element1)) // if element from set 'a' doesn't belong to the set 'b'
            return false;

    return true;
}

// checks if set 'a' and set 'b' are equal
bool isEqual(Relation_t *a, Relation_t *b)
{
    if(a->size != b->size) // equal sets must have the same sizes
        return false;

    // sort both sets

    if(!a->sortedByX)
        sortRelationByX(a);

    if(!b->sortedByX)
        sortRelationByX(b);

    for(int i = 0; i < a->size; i++)
        if(strcmp(a->pair_arr[i].element1.name, b->pair_arr[i].element1.name) != 0) // compare set elements
            return false;

    return true;
}

// checks if set 'a' is a proper subset of set 'b'
bool isProperSubset(Relation_t *a, Relation_t *b)
{
    // set 'a' is a proper subset of set 'b' if it is a subset of set 'b' and sets 'a' and 'b' are not equal
    return !isEqual(a, b) && isSubset(a, b);
}

// returns number of relation pairs that have the same first and second element (i.e. pair is (x, y) where x == y)
int reflexivePairs(Relation_t *r)
{
    int count = 0;

    for(int i = 0; i < r->size; i++)
        if(strcmp(r->pair_arr[i].element1.name, r->pair_arr[i].element2.name) == 0) // if pair is (x, y) and x == y
            count++;

    return count;
}

// on input gets a relation pair (x, y)
// checks if there is a relation pair (y, x) in the relation
bool isPairInRelation(Relation_t *r, char *element1, char *element2)
{
    for(int i = 0; i < r->size; i++)
        if(strcmp(r->pair_arr[i].element1.name, element2) == 0 && strcmp(r->pair_arr[i].element2.name, element1) == 0)
            return true;

    return false;
}

// checks if a relation is a symmetric
bool isSymmetric(Relation_t *r)
{
    if(isEmpty(r))
        return true;

    for(int i = 0; i < r->size; i++)
    {
        // if xRy => than yRx
        if(!isPairInRelation(r, r->pair_arr[i].element1.name, r->pair_arr[i].element2.name))
            return false;
    }

    return true;
}

// checks if a relation is a reflexive
bool isReflexive(Relation_t *r, Relation_t *universal_set)
{
    if(isEmpty(r)) // if a relation is empty it will be reflexive only if a universal set is empty
        return isEmpty(universal_set);

    // if a relation is reflexive is must contain all pairs xRx where x belongs to the universal set
    return reflexivePairs(r) == universal_set->size;
}

// checks if string contains a command over sets
bool isSetCommand(char *str)
{
    for(int i = 0; i < 9; i++)
        if(strcmp(str, set_commands[i]) == 0)
            return true;

    return false;
}

// prints a domain of the relation
// the domain of the relation will be stored in the set 'new'
bool printDomain(Relation_t *r, Relation_t *new)
{
    // firstly sort relation by its first element (by x)
    if(!r->sortedByX)
        sortRelationByX(r);

    for(int i = 0; i < r->size - 1; i++)
    {
        // if the first element (x) is unique add it to the set 'new'
        if(strcmp(r->pair_arr[i].element1.name, r->pair_arr[i + 1].element1.name) != 0)
        {
            if(!resizeAndPairCtor(new, r->pair_arr[i].element1.name, NULL))
                return false;
        }
    }

    if(!isEmpty(r))
    {
        // add the last element
        if(!resizeAndPairCtor(new, r->pair_arr[r->size - 1].element1.name, r->pair_arr[r->size - 1].element2.name))
            return false;
    }

    printSet(new); // print set
    return true;
}

// gets a codomain of the relation
// the codomain of the relation will be stored in the set 'new'
bool printCodomain(Relation_t *r, Relation_t *new, bool isPrint)
{
    // firstly sort relation by its second element (by y)
    if(r->sortedByX)
        sortRelationByY(r);

    for(int i = 0; i < r->size - 1; i++)
    {
        // if the second element (y) is unique add it to the set 'new'
        if(strcmp(r->pair_arr[i].element2.name, r->pair_arr[i + 1].element2.name) != 0)
        {
            if(!resizeAndPairCtor(new, r->pair_arr[i].element2.name, NULL))
                return false;

        }
    }

    if(r->size != 0)
    {
        // add the last element
        if(!resizeAndPairCtor(new, r->pair_arr[r->size - 1].element2.name, NULL))
            return false;
    }

    // if it is needed to print a 'new' set
    if(isPrint)
        printSet(new);

    return true;
}

// checks if a relation is a function
bool isFunction(Relation_t *r)
{
    if(isEmpty(r))
        return true;

    // sort a relation by its first element (by x)
    if(!r->sortedByX)
        sortRelationByX(r);

    for(int i = 0; i < r->size - 1; i++)
    {
        // checks if every first element (x) from the relation is unique
        // i.e. every x in every relation pair is in the relation with another y
        if(strcmp(r->pair_arr[i].element1.name, r->pair_arr[i + 1].element1.name) == 0)
            return false;
    }

    return true;
}


// checks if a relation 'r' is an injective function
// relation 'r' domain is a set 'a', its codomain is a set 'b'
bool isInjective(Relation_t *r, Relation_t *a, Relation_t *b)
{
    if(isEmpty(r)) // if a relation 'r' is empty it can be injective only if both its domain and codomain are empty
        return isEmpty(a) && isEmpty(b);

    if(r->size != a->size) // relation 'r' must have the same size as a relation 'a' (its domain)
        return false;

    // sort relations if they are not sorted by x/y (first and second elements)

    if(r->sortedByX)
        sortRelationByY(r);

    if(!a->sortedByX)
        sortRelationByX(a);

    if(!b->sortedByX)
        sortRelationByX(b);

    // check if x from every relation pair (x, y) belongs to the set 'a' (relation domain)
    // check if y from every relation pair (x, y) belongs to the set 'b' (relation codomain)
    // check if every y from every relation pair (x, y) is unique
    // i.e. every y in every relation pair is in the relation with another x
    for(int i = 0; i < r->size; i++)
        if(!isInSet(a, &r->pair_arr[i].element1) || !isInSet(b, &r->pair_arr[i].element2)
           || (i != r->size - 1 && strcmp(r->pair_arr[i].element2.name, r->pair_arr[i + 1].element2.name) == 0))
            return false;

    return true;
}

// checks if a relation 'r' is a surjective function
// relation 'r' domain is a set 'a', its codomain is a set 'b'
bool isSurjective(Relation_t *r, Relation_t *a, Relation_t *b)
{
    if(isEmpty(r)) // if a relation 'r' is empty it can be surjective only if both its domain and codomain are empty
        return isEmpty(a) && isEmpty(b);

    // relation 'r' size must not be less than size of its domain (set 'a') and its codomain (set 'b')
    if(r->size < a->size || r->size < b->size)
        return false;

    Relation_t rel_codomain; // set that will keep the codomain of the relation 'r'
    relationCtor(&rel_codomain, -1); // initialize it

    if(!printCodomain(r, &rel_codomain, false)) // if error occurred
    {
        relationDtor(&rel_codomain); // free memory
        return false;
    }

    if(!isEqual(&rel_codomain, b)) // compare 'codomains' (they must be the same)
    {
        relationDtor(&rel_codomain); // free memory
        return false;
    }

    // sort a relation
    if(r->sortedByX)
        sortRelationByY(r);

    // check if x from every relation pair (x, y) belongs to the set 'a' (relation domain)
    for(int i = 0; i < r->size; i++)
    {
        if(!isInSet(a, &r->pair_arr[i].element1))
        {
            relationDtor(&rel_codomain);
            return false;
        }
    }

    relationDtor(&rel_codomain); // free memory
    return true;
}

// checks if a relation 'r' is a bijective function
// relation 'r' domain is a set 'a', its codomain is a set 'b'
bool isBijective(Relation_t *r, Relation_t *a, Relation_t *b)
{
    // the relation is bijective if it is both injective and surjective
    return isInjective(r, a, b) && isSurjective(r, a, b);
}

// checks if a relation 'r' is antisymmetric
bool isAntisymmetric(Relation_t *r)
{
    if(isEmpty(r))
        return true;

    // check if for every single relation pair in 'r' works: if xRy and x != y => than y notR x
    for(int i = 0; i < r->size; i++)
        if(isPairInRelation(r, r->pair_arr[i].element1.name, r->pair_arr[i].element2.name) && strcmp(r->pair_arr[i].element1.name, r->pair_arr[i].element2.name) != 0)
            return false;

    return true;
}

// checks if a relation 'r' is transitive
bool isTransitive(Relation_t *r)
{
    for(int i = 0; i < r->size; i++)
    {
        for(int j = 0; j < r->size; j++)
        {
            // check if for every single relation pair in 'r' works: if aRb and bRc => than aRC
            if(strcmp(r->pair_arr[i].element2.name, r->pair_arr[j].element1.name) == 0 && !isPairInRelation(r, r->pair_arr[j].element2.name, r->pair_arr[i].element1.name))
                return false;
        }
    }

    return true;
}

// prints a reflexive closure of the relation 'r'
// the reflexive closure of the relation 'r' will be stored in the relation 'new'
bool printReflexiveClosure(Relation_t *r, Relation_t *new, Relation_t *universal_set)
{
    if(relationCopy(new, r) == NULL) // copy 'r' to 'new'
        return false;

    for(int i = 0; i < universal_set->size; i++)
    {
        // for every single element (x) from the universal set check if there is a relation pair (x, x) in the relation
        // if not => add it to the 'new' relation

        bool isInRelation = false;
        for(int j = 0; j < r->size; j++)
        {
            if(strcmp(universal_set->pair_arr[i].element1.name, r->pair_arr[j].element1.name) == 0 &&
               strcmp(universal_set->pair_arr[i].element1.name, r->pair_arr[j].element2.name) == 0) {
                isInRelation = true;
                break;
            }
        }
        if(!isInRelation)
        {
            if(!resizeAndPairCtor(new, universal_set->pair_arr[i].element1.name, universal_set->pair_arr[i].element1.name))
                return false;
        }
    }

    printRelation(new);
    return true;
}

// prints a symmetric closure of the relation 'r'
// the symmetric closure of the relation 'r' will be stored in the relation 'new'
bool printSymmetricClosure(Relation_t *r, Relation_t *new)
{
    if(relationCopy(new, r) == NULL) // // copy 'r' to 'new'
        return false;

    for(int i = 0; i < r->size; i++)
    {
        // for every single relation pair (x, y) check if there is a relation pair (y, x) in the relation 'r'
        // if not => add it to the 'new' relation

        if(!isPairInRelation(new, new->pair_arr[i].element1.name, new->pair_arr[i].element2.name))
        {
            if(!resizeAndPairCtor(new, new->pair_arr[i].element2.name, new->pair_arr[i].element1.name))
                return false;
        }
    }


    printRelation(new);
    return true;
}

// prints a transitive closure of the relation 'r'
// the transitive closure of the relation 'r' will be stored in the relation 'new'
bool printTransitiveClosure(Relation_t *r, Relation_t *new)
{
    if(relationCopy(new, r) == NULL) // copy 'r' to 'new'
        return false;

    while(1)
    {
        int append_number = 0; // number of added relation pairs to the 'new' relation per one iteration

        int old_size = new->size;

        // if there is a relation pair (a, b) in 'r' and (b, c) => there must be a relation pair (a, c)
        for(int i = 0; i < old_size; i++)
        {
            for(int j = 0; j < old_size; j++)
            {
                if(strcmp(new->pair_arr[i].element2.name, new->pair_arr[j].element1.name) == 0 && !isPairInRelation(new, new->pair_arr[j].element2.name, new->pair_arr[i].element1.name))
                {
                    if(!resizeAndPairCtor(new, new->pair_arr[i].element1.name, new->pair_arr[j].element2.name))
                        return false;

                    append_number++;
                }
            }
        }

        // loop until no changes were done
        if(append_number != 0)
            continue;

        break;
    }

    printRelation(new);
    return true;
}

// checks if string contains a command that prints a set as its output
bool printsSet(char *str)
{
    for(int i = 2; i <= 5; i++)
        if(strcmp(str, set_commands[i]) == 0)
            return true;

    return strcmp(str, relation_commands[5]) == 0 || strcmp(str, relation_commands[6]) == 0;
}

// checks if string contains a command that prints a relation as its output
bool printsRelation(char *operation)
{
    for(int i = 10; i <= 12; i++)
        if(strcmp(operation, relation_commands[i]) == 0)
            return true;

    return false;
}

// prints a random set element/relation pair from the set/relation with id 'id'
bool selectRandom(Relation_arr_t *relation_arr, Relation_arr_t *set_arr, int id, int *skip_lines)
{
    bool isRelation = true;
    Relation_t *r = findById(relation_arr, id); // trying to find a relation by id

    if(r == NULL) // if it couldn't find a relation by id trying to find a set by id
    {
        r = findById(set_arr, id);

        if(r == NULL) // if it couldn't find either set/relation by id
        {
            fprintf(stderr, "Error! Couldn't find a set and relation with specified (%d) id\n", id);
            return false;
        }

        isRelation = false; // if it could find a set by id
    }

    if(isEmpty(r)) // if set/relation is empty there is no need to print anything
        return true;

    int random_idx = rand() % r->size; // get a random index of set element/relation pair

    if(isRelation) // print relation pair
        printf("(%s %s)\n", r->pair_arr[random_idx].element1.name, r->pair_arr[random_idx].element2.name);
    else // print set element
        printf("%s\n", r->pair_arr[random_idx].element1.name);

    *skip_lines = 0; // there is no need to skip any number of lines because set/relation wasn't empty
    return true;
}

// resizes a relation/set and initializes it at index 'a->size - 1' of the relation/set array
bool relationResizeAndCtor(Relation_arr_t *a, int line_cnt)
{
    if(relationArrayResize(a, a->size + 1) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize an array of set\n");
        return false;
    }

    relationCtor(&a->relation_arr[a->size - 1], line_cnt + 1);
    return true;
}

// processes a line with the command and executes it
bool processCommand(char *line, Relation_arr_t *set_arr, Relation_arr_t *relation_arr, int line_cnt, int *skip_lines)
{
    Command_t c = {.operands = {0, }};

    if(!parseOperation(line, &c))
    {
        fprintf(stderr, "Error! Invalid format of the line nc. %d with set/relation operation\n", line_cnt + 1);
        return false;
    }

    // if commands prints true or false as its output or if it is "select" and go_to_line argument is not 0
    if((printsTrueOrFalse(c.name) || strcmp(c.name, relation_commands[13]) == 0) && c.operands[3] != 0)
    {
        // you cant go to line that was already processed because it will cause an infinite loop
        if(line_cnt + 1 >= c.operands[3])
        {
            fprintf(stderr, "Error! Can't continue reading a file from the line nc. %d, because current line has nc. %d\n", c.operands[3], line_cnt + 1);
            return false;
        }

        *skip_lines = c.operands[3] - line_cnt - 2;
    }

    // if the output of the command is set => resize an array of sets in order to save the result of the command
    if(printsSet(c.name))
    {
        if(!relationResizeAndCtor(set_arr, line_cnt))
            return false;
    }
    // if the output of the command is relation => resize an array of relations in order to save the result of the command
    if(printsRelation(c.name))
    {
        if(!relationResizeAndCtor(relation_arr, line_cnt))
            return false;
    }

    // pointers that will hold an addresses of the sets/relations that are command arguments
    Relation_t *r1, *s2, *s3;

    if(strcmp(c.name, relation_commands[13]) != 0) // if command is not "select"
    {
        if(isSetCommand(c.name))
            r1 = findById(set_arr, c.operands[0]); // get a set
        else
            r1 = findById(relation_arr, c.operands[0]); // get a relation

        s2 = findById(set_arr, c.operands[1]); // second mandatory argument is always a set
        s3 = findById(set_arr, c.operands[2]); // third mandatory argument is always a set

        // if it couldn't find a set/relation by specified id
        if(r1 == NULL || (c.operands[1] != 0 && s2 == NULL) || (c.operands[2] != 0 && s3 == NULL))
        {
            fprintf(stderr, "Error! There doesn't exist a set/relation with specified id\n");
            return false;
        }
    }

    // executes needed command

    if(strcmp(c.name, set_commands[0]) == 0)
    {
        if(isEmpty(r1))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, set_commands[1]) == 0)
    {
        printSetSize(r1);
        return true;
    }
    else if(strcmp(c.name, set_commands[2]) == 0)
    {
        return printDifference(set_arr->relation_arr, r1, &set_arr->relation_arr[set_arr->size - 1]);
    }
    else if(strcmp(c.name, set_commands[3]) == 0)
    {
        return printUnion(r1, s2, &set_arr->relation_arr[set_arr->size - 1]);
    }
    else if(strcmp(c.name, set_commands[4]) == 0)
    {
        return printIntersection(r1, s2, &set_arr->relation_arr[set_arr->size - 1]);
    }
    else if(strcmp(c.name, set_commands[5]) == 0)
    {
        return printDifference(r1, s2, &set_arr->relation_arr[set_arr->size - 1]);
    }
    else if(strcmp(c.name, set_commands[6]) == 0)
    {
        if(isSubset(r1, s2))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, set_commands[7]) == 0)
    {
        if(isProperSubset(r1, s2))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, set_commands[8]) == 0)
    {
        if(isEqual(r1, s2))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[0]) == 0)
    {
        if(isReflexive(r1, set_arr->relation_arr))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[1]) == 0)
    {
        if(isSymmetric(r1))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[2]) == 0)
    {
        if(isAntisymmetric(r1))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[3]) == 0)
    {
        if(isTransitive(r1))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[4]) == 0)
    {
        if(isFunction(r1))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[5]) == 0)
    {
        return printDomain(r1, &set_arr->relation_arr[set_arr->size - 1]);
    }
    else if(strcmp(c.name, relation_commands[6]) == 0)
    {
        return printCodomain(r1, &set_arr->relation_arr[set_arr->size - 1], true);
    }
    else if(strcmp(c.name, relation_commands[7]) == 0)
    {
        if(isInjective(r1, s2, s3))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[8]) == 0)
    {
        if(isSurjective(r1, s2, s3))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[9]) == 0)
    {
        if(isBijective(r1, s2, s3))
        {
            printf("%s\n", key_words[1]);
            *skip_lines = 0; // the output of the function is 'ture' => no need to skip any lines
        }
        else
            printf("%s\n", key_words[0]);

        return true;
    }
    else if(strcmp(c.name, relation_commands[10]) == 0)
    {
        return printReflexiveClosure(r1, &relation_arr->relation_arr[relation_arr->size - 1], set_arr->relation_arr);
    }
    else if(strcmp(c.name, relation_commands[11]) == 0)
    {
        return printSymmetricClosure(r1, &relation_arr->relation_arr[relation_arr->size - 1]);
    }
    else if(strcmp(c.name, relation_commands[12]) == 0)
    {
        return printTransitiveClosure(r1, &relation_arr->relation_arr[relation_arr->size - 1]);
    }
    else if(strcmp(c.name, relation_commands[13]) == 0)
    {
        return selectRandom(relation_arr, set_arr, c.operands[0], skip_lines);
    }

    // else

    fprintf(stderr, "Error! Incorrect format of line with set/relation operation\n");
    return false;
}

// frees all allocated memory
void dtor(Relation_arr_t *set_arr, Relation_arr_t *relation_arr, FILE *f)
{
    relationArrayDtor(set_arr);
    relationArrayDtor(relation_arr);
    fclose(f);
}

// checks if string contains a valid parentheses
// it should be like this: ...(...)...(...)...
// i.e. if there is an opening bracket '(' the next bracket must be closing ')'
bool validParentheses(char *str)
{
    int count = 0;

    for(int i = 0; str[i] != '\0'; i++)
    {
        if(str[i] == '(')
            count++;
        else if(str[i] == ')')
            count--;

        if(count != 1 && count != 0)
            return false;
    }

    return count == 0;
}

// checks if a strlen of the set element from the relation pair is from the range [1, 30] inclusively
bool validRelationPair(char *str)
{
    char *open = str + 2; // first '('
    char *close = strchr(str, ')'); // first ')'

    while(open != NULL)
    {
        if(close - open > 62 || close - open < 4)
            return false;

        *close = '\0';
        int space_cnt = numberOfDelimiters(open);
        *close = ')';

        if(space_cnt != 1)
            return false;

        char *delimiter_ptr = strchr(open, DELIMITER_CHAR);

        if(delimiter_ptr - open > 31 || delimiter_ptr - open < 2 || close - delimiter_ptr > 31 || close - delimiter_ptr < 2)
            return false;

        open = strchr(open + 1, '(');
        close = strchr(close + 1, ')');
    }

    return true;
}

// checks if there is an exactly one delimiter (space) between relation pairs
// and if there is a delimiter as the last string character
bool isDelimiterBetweenPair(char *str)
{
    if(str[strcspn(str, "\0") - 1] != ')') // line must end with ')'
        return false;

    char *open = str + 3; // first char after '('
    char *close = strchr(str, ')'); // first ')'
    open = strchr(open, '('); // second '('

    while(open != NULL)
    {
        if(open - close != 2 || *(open - 1) != DELIMITER_CHAR)
            return false;

        open = strchr(open + 1, '(');
        close = strchr(close + 1, ')');
    }

    return true;
}

// parses a relation from the file
bool parseRelation(char *line, Relation_arr_t *relation_arr, int line_cnt, Relation_t *universal)
{
    if(relationArrayResize(relation_arr, relation_arr->size + 1) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize an array of relation_arr\n");
        return false;
    }

    relationCtor(&relation_arr->relation_arr[relation_arr->size - 1], line_cnt + 1);

    if(line[1] == '\0') // empty relation
    {
        printRelation(&relation_arr->relation_arr[relation_arr->size - 1]);
        return true;
    }

    if(!validParentheses(line) || !validRelationPair(line) || ! isDelimiterBetweenPair(line))
    {
        fprintf(stderr, "Error! Invalid format of the line no. %d declaring a relation\n", line_cnt + 1);
        return false;
    }

    // if line declaring a relation has a right format (R (element1 element2) (element1 element2) ...)
    // there are a 'numberOfDelimiters(line) / 2' relation pairs
    // so resize a new set with size 'numberOfDelimiters(line) / 2'
    if(relationResize(&relation_arr->relation_arr[relation_arr->size - 1], numberOfDelimiters(line) / 2) == NULL)
    {
        fprintf(stderr, "Error! Couldn't resize a relation\n");
        return false;
    }

    char *token1 = strtok(line + 3, DELIMITER_STR "(" ")"); // first element (element1, x) from the first relation pair
    char *token2 = strtok(NULL, DELIMITER_STR "(" ")"); // second element (element2, y) from the second relation pair

    int i = 0;

    while(token1 != NULL)
    {
        if(!isValidSetElement(token1) || !isValidSetElement(token2))
        {
            fprintf(stderr, "Error! Invalid set element on line no. %d\n", line_cnt + 1);
            return false;
        }

        if(relationPairCtor(&relation_arr->relation_arr[relation_arr->size - 1].pair_arr[i], token1, token2) == NULL)
        {
            fprintf(stderr, "Error! Couldn't allocate memory for a relation pair\n");
            return false;
        }

        if(line_cnt != 0)
        {
            // check if both set elements from the relation pair belong to the universal set
            if(!isInSet(universal, &relation_arr->relation_arr[relation_arr->size - 1].pair_arr[i].element1) ||
               !isInSet(universal, &relation_arr->relation_arr[relation_arr->size - 1].pair_arr[i].element2))
            {
                fprintf(stderr, "Error! Each element of the relation pair must belong to the universal set\n");
                return false;
            }
        }

        // get next set elements from the next relation pair
        token1 = strtok(NULL, DELIMITER_STR "(" ")");
        token2 = strtok(NULL, DELIMITER_STR "(" ")");
        i++;
    }


    sortRelationByX(&relation_arr->relation_arr[relation_arr->size - 1]);

    if(containDuplicate(&relation_arr->relation_arr[relation_arr->size - 1]))
    {
        fprintf(stderr, "Error! Each relation pair must be unique\n");
        return false;
    }

    printRelation(&relation_arr->relation_arr[relation_arr->size - 1]);
    return true;
}

// processes a file
bool processFile(FILE *f, Relation_arr_t *set_arr, Relation_arr_t *relation_arr)
{
    char line[LINE_BUFFER_SIZE]; // line from the file
    int line_cnt = 0; // number of processed lines from the file
    char last_line; // keeps a first character from the last processed line from the file
    int skip_lines = 0; // how many lines must be skipped

    // initialize array of sets and relations
    relationArrayCtor(set_arr);
    relationArrayCtor(relation_arr);

    // read the file until the end
    while((fgets(line, LINE_BUFFER_SIZE, f)) != NULL)
    {
        if(skip_lines != 0) // if it is needed to skip some lines
        {
            line_cnt++;
            skip_lines--;
            continue;
        }

        if(line_cnt == MAX_LINE_NUMBER)
        {
            fprintf(stderr, "Error! Too many lines in the file\n");
            return false;
        }

        if(!isValidLine(line, line_cnt, &last_line))
            return false;

        if(last_line == 'U' || last_line == 'S')
        {
            if(!parseSet(line, set_arr, line_cnt))
                return false;
        }
        else if(last_line == 'R')
        {
            if(!parseRelation(line, relation_arr, line_cnt, set_arr->relation_arr))
                return false;
        }
        else // if last_line == 'C'
        {
            if(!processCommand(line, set_arr, relation_arr, line_cnt, &skip_lines))
                return false;
        }

        line_cnt++;
    }
    if(skip_lines != 0)
    {
        fprintf(stderr, "Error! Can't continue reading a file from the line no. %d because there are only %d lines in the file\n", line_cnt + skip_lines + 1, line_cnt);
        return false;

    }

    if(last_line != 'C')
    {
        fprintf(stderr, "Error! Invalid file format: it must be like this: U -> R/S -> C\n");
        return false;
    }

    return true;
}

// parses program arguments
bool parseArguments(int argc, char *argv[], FILE **f)
{
    if(argc != 2) // there should be exactly 2 program arguments
    {
        fprintf(stderr, "Error! Invalid number of program arguments\n");
        return false;
    }

    *f = fopen(argv[1], "r");

    if(*f != NULL)
        return true;

    fprintf(stderr, "Error! Couldn't open a file '%s'\n", argv[1]);
    return false;
}

int main(int argc, char *argv[])
{
    FILE *f = NULL;

    if(!parseArguments(argc, argv, &f))
    {
        printUsage();
        return -1;
    }

    Relation_arr_t set_arr, relation_arr;

    srand(time(NULL)); // set a random seed to get truly random numbers each time program is run

    if(!processFile(f, &set_arr, &relation_arr))
    {
        dtor(&set_arr, &relation_arr, f);
        return -1;
    }

    dtor(&set_arr, &relation_arr, f);

    return 0;
}
