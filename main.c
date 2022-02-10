#include "huffman.h"

#include <getopt.h>

/*
 * Main script for Huffman Code
 *
 * Usage:
 *  1. Build with the helpper script (E.g. `gcc main.c huffman.c -o huffman`)
 *  2. Run the script with the options (E.g. `./huffman -m AAAABCCCDDE`)
 */

static void usage(const char *progname);
void encode(FILE *fp, Buffer *buf);
void decode(FILE *fp, bool save);

// Command line options
static const struct option options[4] = {
    {.name = "input", .has_arg = required_argument, .flag = NULL, .val = 'i'},
    {.name = "message", .has_arg = required_argument, .flag = NULL, .val = 'm'},
    {.name = "save", .has_arg = no_argument, .flag = NULL, .val = 's'},
    {.name = "help", .has_arg = optional_argument, .flag = NULL, .val = 'h'},
};

int main(int argc, char *const argv[])
{
  FILE *fp;
  Buffer *buf;
  int opt, idx;
  char const *infile, *message;
  char const *binfile = "out.bin";
  bool save = false;

  // Parse command line arguments if given
  while ((opt = getopt_long(argc, argv, "i:m:sh", options, &idx)) != -1)
  {
    switch (opt)
    {
    case 'i':
      infile = optarg;
      break;
    case 'm':
      message = optarg;
      break;
    case 's':
      save = true;
      break;
    case 'h':
      usage(argv[0]);
      return 0;
    default:
      usage(argv[0]);
      return -1;
      break;
    }
  }

  if (infile)
  {
    printf("[Info]\tReading '%s'\n", infile);
    fp = fopen(infile, "r");
    if (!fp)
      return -1;
    buf = init_buf_from_file(fp);
    if (mem_check(buf, "buf"))
      return -1;
    fclose(fp);
  }
  else if (message)
  {
    printf("[Info]\tReading message\n");
    buf = init_buf_from_str(message);
    if (mem_check(buf, "buf"))
      return -1;
  }
  else
  {
    printf("No input file or message given\n");
    return -1;
  }

  // Encode
  fp = fopen(binfile, "wb");
  if (mem_check(fp, "fp"))
    return -1;
  encode(fp, buf);
  putchar('\n');
  fclose(fp);

  del_buffer(buf);

  // Decode
  printf("[Info]\tReading '%s'\n", binfile);
  fp = fopen(binfile, "rb");
  if (!fp)
    return -1;
  decode(fp, save);
  fclose(fp);
  return 0;
}

/* ************************************************************* */

static void usage(const char *progname)
{
  printf("Usage: %s [OPTION]...\n", progname);
  printf("\n");
  printf("  -i, --input=FILE\n");
  printf("      Specify the input file. (Optional)\n");
  printf("  -m, --message=MESSAGE\n");
  printf("      Specify the message to encode.\n");
  printf("  -s, --save\n");
  printf("      Save the decoded file.\n");
  printf("  -h, --help\n");
  printf("      Display this help and exit.\n");
  printf("\n");
}

void encode(FILE *fp, Buffer *buf)
{
  Tree *tree;

  // Initialize and order leaves by the frequency
  tree = init_tree_from_buf(buf);

  printf("[Info]\tOccurrences: ");
  for (Node *head = tree->root; head; head = head->next)
    printf("{%#x: %zu times} ", head->symbol, head->freqs);
  putchar('\n');

  // Assign leaves to the tree
  tree = build_tree(tree);

  CodeBook *book = tree2book(tree);
  for (uint8_t i = 0; i < tree->num_symbols; i++)
    print_table(book->table[i]);

  // Write to file
  compress(fp, buf, book);

  // del_tree(tree);
  del_codebook(book);
  del_tree(tree);
}

void decode(FILE *fp, bool save)
{
  // Read the compression info
  uint8_t byte; // Temporary octet storage

  // Read the number of symbols
  uint8_t *sign = (uint8_t *)malloc(sizeof(uint8_t));
  fread(sign, sizeof(uint8_t), FILE_SIGN_LEN, fp);

  // Check the signature
  if (memcmp(sign, FILE_SIGN, FILE_SIGN_LEN))
  {
    fprintf(stderr, "[Error]\tInvalid signature\n");
    return;
  }
  free(sign);

  // Read codebook
  CodeBook *book = read_codebook(fp);

  // Check if the codebook is finished
  fread(&byte, sizeof(uint8_t), 1, fp);
  if (byte != GROUP_SEPARATOR)
  {
    fprintf(stderr, "[Error]\tInvalid file header\n");
    return;
  }
  byte = 0x00;

  // Read the original length in bytes
  uint64_t origin_len;
  fread(&origin_len, sizeof(uint64_t), 1, fp);

  fread(&byte, sizeof(uint8_t), 1, fp);
  if (byte != GROUP_SEPARATOR)
  {
    fprintf(stderr, "[Error]\tInvalid file header\n");
    return;
  }
  byte = 0x00;

  // Decompress the data
  uint32_t bits = 0;
  uint8_t bit_count = 0;
  size_t data_len = 0;
  uint8_t data[origin_len];
  while (fread(&byte, sizeof(uint8_t), 1, fp) == 1)
  {
    if (byte == GROUP_SEPARATOR)
      break;

    for (int i = 7; 0 <= i; i--)
    {
      uint32_t mask = 1 << i;
      uint32_t bit_i = (byte & mask) >> i;

      bits <<= 1;
      bits |= bit_i;
      bit_count++;

      CodeTable *table = search_code(book, bits, bit_count);
      if (table)
      {
        // printf("%c %s\n", table->symbol, bitstr(bits, bit_count));
        data[data_len] = table->symbol;
        data_len++;
        bits = 0;
        bit_count = 0;
      }
    }
  }

  // Read the data length in bits
  uint64_t total;
  fread(&total, sizeof(uint64_t), 1, fp);

  // Check if the EOF is reached
  if (feof(fp))
  {
    fprintf(stderr, "[Error]\tInvalid file format\n");
    return;
  }

  // Write the data
  if (save)
  {
    FILE *out = fopen("out.txt", "w");
    printf("[Info]\tWriting to 'out.txt'\n");
    fwrite(data, sizeof(uint8_t), data_len, out);
    fclose(out);
  }
  else
    printf("\n>>> %s\n", data);

  del_codebook(book);
}
