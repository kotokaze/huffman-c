#pragma once
#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#define FILE_SIGN "HUFFBOOK"
#define FILE_SIGN_LEN (strlen(FILE_SIGN))

#define GROUP_SEPARATOR 0x29

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ******************************************** */

// Get the binary representation of number
uint8_t *bitstr(unsigned num, uint8_t len);

// Check if the memory is valid
int mem_check(void *ptr, const char *name);

/* ******************************************** */

// Buffer to store the input
typedef struct buffer_t
{
  uint8_t *buffer;   // buffer
  uint64_t len;      // length of the data
  uint64_t capacity; // capacity of the buffer
} Buffer;

// Initialize a stream buffer from a file
Buffer *init_buf_from_file(FILE *fp);

// Initialize a stream buffer from a string
Buffer *init_buf_from_str(const char *str);

// Free the buffer
int del_buffer(Buffer *buf);

/* ******************************************** */

// Node of the huffman tree
typedef struct huffman_tree_node_t
{
  // The symbol represents this node
  uint8_t symbol;

  // The number of times this symbol appears
  size_t freqs;

  // Whether this node is a leaf
  bool is_leaf;

  // The pointer to the next node for the linked list
  struct huffman_tree_node_t *next;

  // The left and right leaves of this node
  struct huffman_tree_node_t *children[2];
} Node;

// Free the node elements
int del_node(Node *node);

/* ******************************************** */

// Pointer to the root, and the length of the tree
typedef struct huffman_tree_t
{
  Node *root;
  size_t num_symbols;
} Tree;

// Initialize leaves of the tree
Tree *init_tree_from_buf(Buffer *buf);

// Build huffman tree
Tree *build_tree(Tree *tree);

// Free the tree
int del_tree(Tree *tree);

/* ******************************************** */

// Table to store the huffman code
typedef struct codetable_t
{
  uint8_t symbol;
  uint32_t code;
  uint8_t num_bits;
} CodeTable;

// Allocate a new codetable
CodeTable *new_codetable();

// Print the codetable
void print_table(CodeTable *code);

/* ******************************************** */

// Array of codetable, and the number of tables
typedef struct codebook_t
{
  CodeTable **table;    // array of codetable
  uint64_t num_symbols; // number of symbols
} CodeBook;

// Allocate a new codebook
CodeBook *new_codebook();

// Create a codebook from the tree
CodeBook *tree2book(Tree *tree);

// Search the codebook for the symbol
CodeTable *search_symbol(CodeBook *book, uint8_t symbol);

// Search the codebook for the code
CodeTable *search_code(CodeBook *book, uint32_t code, uint8_t num_bits);

// Free the codebook
int del_codebook(CodeBook *book);

/* ******************************************** */

void compress(FILE *fp, Buffer *buf, CodeBook *book);

CodeBook *read_codebook(FILE *fp);
Buffer *read_bitdata(FILE *fp, uint64_t *len);

#endif // __HUFFMAN_H__
