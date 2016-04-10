#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h> 
#include <stdint.h>
#include <stdbool.h>
#include <string.h> 
#include <math.h>

#define BUFFER_SIZE 8192
#define BITS_IN_BLOCK 32

typedef struct qtreeNode qtreeNode;
typedef struct ARCH ARCH;
typedef struct codeInfo codeInfo;
typedef struct archiveInfo archiveInfo;

struct archiveInfo {
    uint8_t tableLength;
    uint32_t numberOfBlocks;
    uint32_t remainingBits;
};

struct codeInfo {
    uint8_t character;
	uint8_t length;
	uint32_t code;
};

struct qtreeNode {
    qtreeNode *rchild;
    qtreeNode *lchild;
    qtreeNode *nextNode;
    uint32_t weight;
    uint8_t symb;
};

struct ARCH {
    qtreeNode *head;
    qtreeNode *tail;
    qtreeNode *root;
    codeInfo codes[256];
    archiveInfo archInfo;
    uint8_t numberOfCodes;
};


bool compress(ARCH* self, const char* dstFileName, const char* srcFileName);
bool decompress(ARCH* self, const char* dstFileName, const char* srcFileName);
ARCH* initArch(void);

#endif
