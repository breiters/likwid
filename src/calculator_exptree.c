// #define eprintf(...) fprintf(stderr, __VA_ARGS__)

#include <bstrlib.h>
#include <bstrlib_helper.h>
#include <perfgroup.h> /* CounterList */

#include "calculator_exptree.h"

// #include <assert.h>
#include <ctype.h>
#include <inttypes.h>
// #include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct exptree_node {
    struct exptree_node *left;  // Left child
    struct exptree_node *right; // Right child

    double value; // Operand value (if it's a number)
    char  *counter_name;
    char   operator; // Operator: '+', '-', '*', '/'
};

// Forward declarations
static struct exptree_node *_make_expression_tree(const char **expr);
static struct exptree_node *_make_term_tree(const char **expr);
static struct exptree_node *_make_factor_tree(const char **expr);

#define NODE_NULL_VALUE    0.0
#define NODE_NULL_OPERATOR '\0'

static void _skip_spaces(const char **expr)
{
    while (isspace(**expr)) {
        (*expr)++;
    }
}

// Set value and create a leaf node
static struct exptree_node *_make_value_node(double value)
{
    struct exptree_node *node = malloc(sizeof(struct exptree_node));
    if (!node) {
        return NULL;
    }
    *node = (struct exptree_node){.left         = NULL,
                                  .right        = NULL,
                                  .value        = value,
                                  .counter_name = NULL,
                                  .operator= NODE_NULL_OPERATOR };
    return node;
}

// Set counter and create a leaf node
static struct exptree_node *_make_counter_node(char *counter)
{
    struct exptree_node *node =
        (struct exptree_node *)malloc(sizeof(struct exptree_node));
    if (!node) {
        return NULL;
    }
    *node = (struct exptree_node){.left         = NULL,
                                  .right        = NULL,
                                  .value        = NODE_NULL_VALUE,
                                  .counter_name = counter,
                                  .operator= NODE_NULL_OPERATOR };
    return node;
}

// Parse an operator and create an operator node
static struct exptree_node *
_make_operator_node(char operator, struct exptree_node * left, struct exptree_node *right)
{
    struct exptree_node *node =
        (struct exptree_node *)malloc(sizeof(struct exptree_node));
    if (!node) {
        return NULL;
    }
    *node = (struct exptree_node){.left         = left,
                                  .right        = right,
                                  .value        = NODE_NULL_VALUE,
                                  .counter_name = NULL,
                                  .operator= operator};
    return node;
}

// Parse factors: numbers or subexpressions in parentheses
static struct exptree_node *_make_factor_tree(const char **expr)
{
    _skip_spaces(expr);
    if (**expr == '(') {
        (*expr)++; // Skip '('
        // Recursively parse the subexpression:
        struct exptree_node *subtree = _make_expression_tree(expr);
        _skip_spaces(expr);
        if (**expr == ')') {
            (*expr)++; // Skip ')'
        } else {
            fprintf(stderr, "Error: Mismatched parentheses\n");
            exit(EXIT_FAILURE);
        }
        return subtree;
    } else {
        char  *endptr;
        double value = strtod(*expr, &endptr);
        if (*expr == endptr) {
            // no conversion performed
            char *counter;
            if (sscanf(*expr, " %m[^()+-*/ \n] %*s", &counter) == 1) {
                *expr += strlen(counter);
                return _make_counter_node(counter);
            } else {
                fprintf(stderr, "Error: Could not parse: %s\n", *expr);
                exit(EXIT_FAILURE);
            }
        }
        *expr = endptr;
        return _make_value_node(value);
    }
}

// Parse terms: handles multiplication and division
static struct exptree_node *_make_term_tree(const char **expr)
{
    struct exptree_node *left = _make_factor_tree(expr);
    while (1) {
        _skip_spaces(expr);
        if (**expr == '*' || **expr == '/') {
            char operator= ** expr;
            (*expr)++;
            struct exptree_node *right = _make_factor_tree(expr);
            left                       = _make_operator_node(operator, left, right);
        } else {
            break;
        }
    }
    return left;
}

// Parse expressions: handles addition and subtraction
static struct exptree_node *_make_expression_tree(const char **expr)
{
    struct exptree_node *left = _make_term_tree(expr);
    while (1) {
        _skip_spaces(expr);
        if (**expr == '+' || **expr == '-') {
            char operator= ** expr;
            (*expr)++;
            struct exptree_node *right = _make_term_tree(expr);
            left                       = _make_operator_node(operator, left, right);
        } else {
            break;
        }
    }
    return left;
}

struct exptree_node *make_expression_tree(const char *expr)
{
    return _make_expression_tree(&expr);
}

// Print the expression tree in in-order traversal
static void _print_expression_tree(const struct exptree_node *node)
{
    if (!node) {
        return;
    }
    if (node->operator) {
        printf("(");
    }
    _print_expression_tree(node->left);
    if (node->operator) {
        printf(" %c ", node->operator);
    } else if (node->counter_name) {
        printf("%s", node->counter_name);
    } else {
        printf("%g", node->value);
    }
    _print_expression_tree(node->right);
    if (node->operator) {
        printf(")");
    }
}

// Print the expression tree in in-order traversal
void print_expression_tree(const struct exptree_node *node)
{
    if (!node) {
        printf("Empty expression tree\n");
        return;
    }
    _print_expression_tree(node);
    printf("\n");
}

// Free the memory used by the tree
void free_expression_tree(struct exptree_node *node)
{
    if (!node) {
        return;
    }
    free_expression_tree(node->left);
    free_expression_tree(node->right);
    free(node->counter_name);
    free(node);
}

static double _get_value(const struct exptree_node *node, const CounterList *clist)
{
    if (!node->counter_name) {
        return node->value;
    }

    size_t len = strlen(node->counter_name);
    // printf("node->counter: %s\n", node->counter);

    /* TODO: set counter index when making the counter node to avoid redundant search */
    /*       only ok if order does not change */
    for (int ctr = 0; clist->counters; ++ctr) {
        const char *cname = bdata(clist->cnames->entry[ctr]);
        // printf("cname*: %s\n", cname);
        if (len == strlen(cname) && !strncmp(node->counter_name, cname, len)) {
            // printf("counter found: ctr=%d\n", ctr);
            const char *val_str = bdata(clist->cvalues->entry[ctr]);
            /* TODO: why are counter values stored as strings instead of unsigned long
             * long ? */
            // printf("cname: %s\n", cname);
            // printf("val_str: %s\n", val_str);
            double val = strtod(val_str, NULL);
            /* TODO error handling of strtod */
            return val;
        }
    }

    eprintf("counter not found: %s\n", node->counter_name);
    return NODE_NULL_VALUE; // TODO
}

// Evaluate the expression tree recursively
double evaluate_expression_tree(const struct exptree_node *node, const CounterList *clist)
{
    // TODO: maybe return NAN to indicate error ? need to check for NULL in child nodes
    if (!node) {
        return 0.0;
    }

    // If it's a leaf node (number/counter), return its value
    if (node->operator== NODE_NULL_OPERATOR) {
        return _get_value(node, clist);
    }

    // Recursively evaluate left and right subtrees
    double val_left  = evaluate_expression_tree(node->left, clist);
    double val_right = evaluate_expression_tree(node->right, clist);

    // Apply the operator
    switch (node->operator) {
    case '+':
        return val_left + val_right;
    case '-':
        return val_left - val_right;
    case '*':
        return val_left * val_right;
    case '/':
        if (val_right == 0.0) {
            fprintf(stderr, "Error: Division by zero\n");
            exit(EXIT_FAILURE);
        }
        return val_left / val_right;
    default:
        fprintf(stderr, "Error: Unknown operator '%c'\n", node->operator);
        exit(EXIT_FAILURE);
    }
}

#if 0
typedef double (*binop_fn_type)(double, double);

struct exptree_node {
    double               value;
    char                *event;
    struct exptree_node *left;
    struct exptree_node *right;
    struct exptree_node *parent;
    binop_fn_type        fn;
};

struct measurement {
    char     **event_names;
    long long *values;
    double     time;
};

struct user_formula {
    char                *metric;
    char                *unit;
    struct exptree_node *root;
};

static int get_formula(struct user_formula *f, const char *formula)
{
#    if 0
    char *formula;
    // parse formula string
    if (sscanf(string,
               " %255m[^[] [%255m[^]] %*[^=]= %255m[^\n]",
               &(f->metric),
               &(f->unit),
               &formula) != 3) {
        return PAPI_UTIL_PARSE_ERROR;
    }
#    endif
#endif