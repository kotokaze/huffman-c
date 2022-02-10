#include "huffman.h"

// #define __DEBUG__

int mem_check(void *ptr, const char *name)
{
  if (!ptr)
  {
    fprintf(stderr, "Failed to allocate %s\n", name);
    return -1;
  }

#if defined(__DEBUG__)
  printf("[Debug]\t'%s' is on %p\n", name, ptr);
#endif // __DEBUG__

  return 0;
}

uint8_t *bitstr(unsigned num, uint8_t len)
{
  uint8_t *str = (uint8_t *)malloc(len * sizeof(uint8_t));
  if (mem_check(str, "str"))
    return NULL;

  for (uint8_t i = 0; i < len; i++)
    str[len - i - 1] = (num & (1 << i)) ? '1' : '0';
  return str;
}

/* ************************************************** */

// Allocate a new buffer
Buffer *new_buffer(uint64_t capacity);

// Add a byte to the buffer
Buffer *add_buffer(Buffer *buf, uint8_t byte);

// Get the list of unique symbols
uint8_t *init_symbol_list(Buffer *buf, size_t *counter);

// Allocate a new node
Node *new_node(uint8_t symbol, size_t freqs);

// Add a new node to the tree
Node *add_node(Node *root, Node *node);

// Allocate a new codebook
CodeBook *new_codebook();

// Depth-first traversal of the tree
void dfs(CodeBook *book, Node *node, uint32_t code, uint8_t len);

// Read codebook from the file header
CodeBook *load_codebook(FILE *fp);

/* ************************************************** */

Buffer *new_buffer(uint64_t capacity)
{
  Buffer *buf = (Buffer *)malloc(sizeof(Buffer));
  if (mem_check(buf, "buf"))
    return NULL;

  buf->buffer = (uint8_t *)malloc(capacity * sizeof(uint8_t));
  if (mem_check(buf->buffer, "buf->buffer"))
    return NULL;

  buf->len = 0;
  buf->capacity = capacity;
  return buf;
}

Buffer *add_buffer(Buffer *buf, uint8_t byte)
{
  if (buf->len == buf->capacity)
  {
    buf->capacity *= 2;
    buf->buffer = (uint8_t *)realloc(buf->buffer, buf->capacity * sizeof(uint8_t));
    if (mem_check(buf->buffer, "buf->buffer"))
      return NULL;
  }

  buf->buffer[++buf->len] = byte;

  return buf;
}

Buffer *init_buf_from_file(FILE *fp)
{
  Buffer *buf;

  // Get file size
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp) - 1;
  size_t capacity = (len * sizeof(uint8_t)) * 2;

  // Rewind to beginning of file
  if (fseek(fp, 0, SEEK_SET))
    return NULL;

  // Allocate buffer
  buf = new_buffer(capacity);
  if (mem_check(buf, "buf"))
    return NULL;

  // Read file into buffer
  size_t read = fread(buf->buffer, sizeof(uint8_t), len, fp);
  if (read != len)
    return NULL;

  // Set data length
  buf->len = len;

  // Show the buffer
  printf("[Info]\tInitialized with %lld octets: %s\n", buf->len * sizeof(uint8_t), buf->buffer);
  return buf;
}

Buffer *init_buf_from_str(const char *str)
{
  Buffer *buf;

  // Get string length
  size_t len = strlen(str);

  // Allocate buffer
  buf = new_buffer(len);
  if (mem_check(buf, "buf"))
    return NULL;

  // Copy string into buffer
  memcpy(buf->buffer, str, len);

  // Set data length
  buf->len = len;

  // Show the buffer
  printf("[Info]\tInitialized with %lld octets: %s\n", buf->len * sizeof(uint8_t), buf->buffer);
  return buf;
}

int del_buffer(Buffer *buf)
{
  if (!buf)
    return -1;

  free(buf->buffer);
  free(buf);
  return 0;
}

Node *new_node(uint8_t symbol, size_t freqs)
{
  Node *node = (Node *)malloc(sizeof(Node));
  if (mem_check(node, "node"))
    return NULL;

  node->symbol = symbol;
  node->freqs = freqs;
  node->is_leaf = false;
  node->next = NULL;
  node->children[0] = NULL;
  node->children[1] = NULL;
  return node;
}

int del_node(Node *node)
{
  if (node->children[0])
    del_node(node->children[0]);
  if (node->children[1])
    del_node(node->children[1]);
  free(node);
  return 0;
}

Node *add_node(Node *root, Node *node)
{
#if defined(__DEBUG__)
  printf("[DEBUG]\tAdding %c (%#x)\n", node->symbol, node->symbol);
#endif // __DEBUG__
  if (!root)
    root = node;

  else if (node->freqs <= root->freqs)
  {
    node->next = root;
    root = node;
  }

  else
  {
    Node *head = root;
    while (head->next && head->next->freqs < node->freqs)
      head = head->next;
    node->next = head->next;
    head->next = node;
  }

  return root;
}

uint8_t *init_symbol_list(Buffer *buf, size_t *counter)
{
  uint8_t *list;
  size_t count = *counter;

  list = (uint8_t *)malloc(buf->len * sizeof(uint8_t));
  if (mem_check(list, "symbols"))
    return NULL;

  for (size_t i = 0; i < buf->len; i++)
  {
    uint8_t c = buf->buffer[i];
    if (count == 0 || c != list[count - 1])
      list[count++] = c;
  }

  // Adjust the memory size
  list = realloc(list, count * sizeof(uint8_t));
  if (mem_check(list, "list"))
    return NULL;

  *counter = count;
  return list;
}

Tree *init_tree_from_buf(Buffer *buf)
{
  Tree *tree = (Tree *)malloc(sizeof(Tree));
  if (mem_check(tree, "tree"))
    return NULL;

  size_t *num_symbols = &tree->num_symbols;              // Number of unique symbols
  uint8_t *symbols = init_symbol_list(buf, num_symbols); // Symbol list
  if (mem_check(symbols, "symbols"))
    return NULL;

  // Print the symbols
  printf("[Info]\tUnique symbols(%zu): ", *num_symbols);
  for (size_t i = 0; i < *num_symbols; i++)
    printf("%#x ", symbols[i]);
  printf("\n");

  // Count the number of occurrences of each symbol
  size_t *occurs = (size_t *)calloc(*num_symbols, sizeof(size_t));
  if (mem_check(occurs, "occurs"))
    return NULL;
  for (size_t i = 0; i < buf->len; i++)
  {
    uint8_t c = buf->buffer[i];
    for (size_t j = 0; j < *num_symbols; j++)
      if (c == symbols[j])
      {
        occurs[j]++;
        break;
      }
  }

  // Create linked list of leaf nodes
  Node *root = NULL;
  for (size_t i = 0; i < *num_symbols; i++)
  {
    Node *leaf = new_node(symbols[i], occurs[i]);
    if (mem_check(leaf, "leaf"))
      return NULL;
    leaf->is_leaf = true;
    root = add_node(root, leaf);
  }

  tree->root = root;

  // Free the memory
  free(symbols);
  free(occurs);
  return tree;
}

Tree *build_tree(Tree *tree)
{
  Node *head;
  for (head = tree->root; head->next; head = head->next)
  {
    Node *root = NULL;

    // Create a root (`0x00` for NUL)
    root = new_node(0x00, (head->freqs + head->next->freqs));
    if (mem_check(root, "root"))
      return NULL;

    root->children[0] = head;
    root->children[1] = head->next;

#if defined(__DEBUG__)
    Node **children = root->children;
    printf("[DEBUG]\tAdding Node((%c:%zu), children:", root->symbol, root->freqs);
    printf(" {0: (%c:%zu),", children[0]->symbol, children[0]->freqs);
    printf(" 1: (%c:%zu)})", children[1]->symbol, children[1]->freqs);
    printf("\n");
#endif // __DEBUG__

    // Prepare for the next iteration
    head->next = head->next->next;
    if (head->children[0] && head->children[0]->next)
      head->children[0]->next = NULL;
    if (head->children[1] && head->children[1]->next)
      head->children[1]->next = NULL;

    head = add_node(head, root);
  }

  tree->root = head;
  return tree;
}

int del_tree(Tree *tree)
{
  del_node(tree->root);
  free(tree);
  return 0;
}

CodeTable *new_codetable()
{
  CodeTable *table = (CodeTable *)malloc(sizeof(CodeTable));
  if (mem_check(table, "code"))
    return NULL;

  table->symbol = 0x00; // NUL
  table->code = 0;
  table->num_bits = 0;
  return table;
}

void dfs(CodeBook *book, Node *node, uint32_t code, uint8_t len)
{
  if (node->is_leaf)
  {
    CodeTable *tb = new_codetable();
    if (mem_check(tb, "table"))
      return;

    tb->symbol = node->symbol;
    tb->code = code;
    tb->num_bits = len;
    size_t idx = (++book->num_symbols) - 1;
    book->table[idx] = tb;
  }
  else
  {
    len++;      // Increase the length of the code
    code <<= 1; // Shift left by 1 bit

    dfs(book, node->children[0], code, len);
    dfs(book, node->children[1], code + 1, len);
  }
}

void print_table(CodeTable *table)
{
  printf("[Info]\tCodeTable(");
  printf("symbol=%#x, ", table->symbol);
  printf("code=%s, ", bitstr(table->code, table->num_bits));
  printf("num_bits=%u", table->num_bits);
  printf(")\n");
}

CodeBook *new_codebook()
{
  CodeBook *book;

  book = (CodeBook *)malloc(sizeof(CodeBook));
  if (mem_check(book, "book"))
    return NULL;

  book->table = NULL;
  book->num_symbols = 0;
  return book;
}

int del_codebook(CodeBook *book)
{
  if (!book)
    return 0;

  if (*book->table)
    for (size_t i = 0; i < book->num_symbols; i++)
      if (book->table[i])
        free(book->table[i]);

  free(book->table);
  free(book);
  return 0;
}

CodeBook *tree2book(Tree *tree)
{
  CodeBook *book;
  book = new_codebook();
  book->table = (CodeTable **)calloc(tree->num_symbols, sizeof(CodeTable *));
  if (mem_check(book->table, "table"))
    return NULL;

  dfs(book, tree->root, 0, 0);
  return book;
}

CodeTable *search_symbol(CodeBook *book, uint8_t symbol)
{
  for (uint8_t i = 0; i < book->num_symbols; i++)
    if (book->table[i]->symbol == symbol)
      return book->table[i];
  return NULL;
}

CodeTable *search_code(CodeBook *book, uint32_t code, uint8_t len)
{
  // printf("[Info]\tSearching code: %s\n", bitstr(code, len));
  for (uint8_t i = 0; i < book->num_symbols; i++)
    if (book->table[i]->code == code && book->table[i]->num_bits == len)
      return book->table[i];
  return NULL;
}

/* ******************************************* */

void write_bit(uint8_t bit, FILE *fp)
{
  static uint8_t byte = 0x00;
  static uint8_t bit_idx = 8;

  byte |= (bit << --bit_idx);
  if (bit_idx == 0)
  {
    printf("[Info]\tWriting byte: %s\n", bitstr(byte, 8));
    fwrite(&byte, sizeof(uint8_t), 1, fp);
    byte = 0x00;
    bit_idx = 8;
  }
}

void write_code(CodeTable *table, FILE *fp)
{
  uint8_t bit;
  for (uint8_t i = 0; i < table->num_bits; i++)
  {
    bit = (table->code >> (table->num_bits - i - 1)) & 0x01;
    write_bit(bit, fp);
  }
}

void compress(FILE *fp, Buffer *buf, CodeBook *book)
{
  const char sep = GROUP_SEPARATOR; // GS (Group Separator)

  // Write the file header
  const char *sign = FILE_SIGN;
  fwrite(sign, sizeof(uint8_t), FILE_SIGN_LEN, fp);

  // Write the codebook
  fwrite(&book->num_symbols, sizeof(uint64_t), 1, fp);

  for (size_t i = 0; i < book->num_symbols; i++)
  {
    size_t bytes = book->table[i]->num_bits / 8 + ((book->table[i]->num_bits % 8 != 0) ? 1 : 0);
    fwrite(&book->table[i]->symbol, sizeof(uint8_t), 1, fp);
    fwrite(&book->table[i]->num_bits, sizeof(uint8_t), 1, fp);
    fwrite(&book->table[i]->code, sizeof(uint8_t), bytes, fp);
  }

  fwrite(&sep, sizeof(uint8_t), 1, fp); // End of codebook

  // Write the original length
  fwrite(&buf->len, sizeof(uint64_t), 1, fp);

  // Write the separator
  fwrite(&sep, sizeof(uint8_t), 1, fp); // End of header

  // Write the compressed data as a bitsream
  uint64_t count = 0;
  for (size_t i = 0; i < buf->len; i++)
  {
    CodeTable *table = search_symbol(book, buf->buffer[i]);
    if (table)
    {
      count += table->num_bits;
      write_code(table, fp);
    }
  }

  // Write the separator
  fwrite(&sep, sizeof(uint8_t), 1, fp); // End of data

  // Write the number of bis written
  fwrite(&count, sizeof(uint64_t), 1, fp);

  // Show the statistics
  putchar('\n');
  double avg = (double)count / (double)buf->len;
  printf("Average: %.2f [bits/symbol]\n", avg);
  printf("Compression ratio: %.1f%% ", (100 * avg / 8.0));
  printf("(In case all inputs are 8-bit)\n");
}

/* ******************************************** */

CodeBook *read_codebook(FILE *fp)
{
  CodeBook *book;
  book = new_codebook();
  if (mem_check(book, "book"))
    return NULL;

  fseek(fp, FILE_SIGN_LEN, SEEK_SET);

  // Read the number of symbols
  fread(&book->num_symbols, sizeof(uint64_t), 1, fp);

  book->table = (CodeTable **)malloc(book->num_symbols * sizeof(CodeTable *));
  if (mem_check(book->table, "book->table"))
    return NULL;

  // Read the codetable
  for (uint64_t i = 0; i < book->num_symbols; i++)
  {
    book->table[i] = new_codetable();
    if (mem_check(book->table[i], "table"))
      return NULL;

    // Read the symbol
    fread(&book->table[i]->symbol, sizeof(uint8_t), 1, fp);

    // Read the number of bits in the code
    fread(&book->table[i]->num_bits, sizeof(uint8_t), 1, fp);

    // Read the code
    size_t bytes = (book->table[i]->num_bits / 8) + ((book->table[i]->num_bits % 8 != 0) ? 1 : 0);
    fread(&book->table[i]->code, bytes, 1, fp);

    print_table(book->table[i]);
  }

  return book;
}

/* ******************************************** */

// #define __TEST__
#ifdef __TEST__
int main(void)
{
  Node *root = NULL;
  uint8_t *strings = (unsigned char *)"$abbccc";
  size_t len = strlen((const char *)strings);

  for (size_t i = 0; i < len; i++)
  {
    uint8_t c = strings[i];
    Node *node = new_node(c, len - i);
    if (mem_check(node, "node"))
      return -1;
    root = add_node(root, node);
  }
  print_node_list(root);
}
#endif // __TEST__
