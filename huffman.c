#include "huffman.h"

static qtreeNode* initQTreeNode(void);
static inline void checkNodeForSymbol(qtreeNode*, uint8_t, uint32_t, ARCH*);
static inline void freeNode(qtreeNode*, uint8_t, uint32_t, ARCH*);
static inline bool addElementToQueue(ARCH*, uint8_t, uint32_t);
static uint32_t reverse_bits(uint32_t, uint32_t);
static void traverseTree(qtreeNode*, void (*)(qtreeNode*, uint8_t, uint32_t, ARCH*), 
                            uint8_t, uint32_t, ARCH*);
static bool insertToQueue(ARCH*, qtreeNode*, qtreeNode*, bool);
static bool rebuildTree(ARCH*, FILE*);
static bool buildTree(ARCH*);
static bool buildQueue(ARCH*, const char*);
static bool generateCodeTable(ARCH*);
static bool fileDecode(ARCH*, FILE*, FILE*);
static bool writeDataToFile(ARCH*, const char*, const char*);
static bool writeCodesToFile(ARCH*, const char*);
static bool writeArchiveInfo(ARCH*, const char*);
static bool readArchiveInfo(ARCH*, FILE*);

/*
This function inserts an <src> node at the position
of the <dst> node. <rightInsert> flag used for the cases
when the <dst> node is the the head of the queue and we need
to determine should the <src> node be appended or prepended. 
*/
static bool 
insertToQueue (ARCH* self, 
               qtreeNode* dst, 
               qtreeNode* src, 
               bool leftInsert) 
{
    if (self->head == NULL) {
        self->head = self->tail = src;
    } else if (dst == self->head) {
        if (leftInsert) {
            if (self->head == self->tail) {
                self->tail = src;
                self->head->nextNode = self->tail;
            } else {
                src->nextNode = self->head->nextNode;
                self->head->nextNode = src;
            }
        } else {
            src->nextNode = self->head;
            self->head = src;
        }
    } else if (dst == self->tail) {
        self->tail->nextNode = src;
        src->nextNode = NULL;
        self->tail = src;
    } else {
        src->nextNode = dst->nextNode;
        dst->nextNode = src;
    }

    return true;
}

/*
This function adds a node to the queue. Because we should always
keep the queue sorted in the order of rising node`s weight, the
position of the new node determines by it`s weight.
*/
static inline bool 
addElementToQueue (ARCH* self, 
                   uint8_t symb, 
                   uint32_t value) 
{
    qtreeNode *cPtr = self->head;
    qtreeNode *newNode = initQTreeNode();

    newNode->symb = symb;
    newNode->weight = value;

    if (cPtr != NULL)  {
        while (cPtr->nextNode != NULL) {
            if (cPtr->nextNode->weight > value) {
                break;
            }

            cPtr = cPtr->nextNode;
        }

        insertToQueue(self, cPtr, newNode, (bool)(cPtr->weight < value));
    } else {
        insertToQueue(self, cPtr, newNode, true);
    }    

    return true;
}

static bool 
buildQueue (ARCH* self, 
            const char* srcFileName)
{
    uint32_t symbols[256] = {0};
    uint8_t buff[BUFFER_SIZE] = {0};
    uint32_t readedChars;
    uint32_t i;

    FILE *text = fopen(srcFileName, "r");
    
    while ((bool)(readedChars = fread(buff, sizeof(uint8_t), BUFFER_SIZE, text))) {
        for (i = 0; i < readedChars; ++i) {
            symbols[buff[i]]++;    
        }
    }

    for (i = 0; i < 256; i++) {
        if (symbols[i] > 0) {
            addElementToQueue(self, i, symbols[i]);
        }
    }
    
    fclose(text);
    return true;
}

static bool 
buildTree(ARCH* self)
{
    qtreeNode *firstNode, *secondNode, *newNode, *cPtr;

    if (self->head == self->tail) {
        self->root = initQTreeNode();
        self->root->lchild = self->head;

        return true;
    }

    while (self->head != self->tail) {     
        firstNode = self->head;
        secondNode = firstNode->nextNode;
        newNode = initQTreeNode();

        newNode->weight = firstNode->weight + secondNode->weight;
        newNode->lchild = firstNode;
        newNode->rchild = secondNode;
        newNode->nextNode = secondNode->nextNode;

        if (self->head->nextNode != self->tail) {
            if (newNode->nextNode->weight < newNode->weight) {
                self->head = newNode->nextNode;
                cPtr = self->head;

                while (cPtr->nextNode && (cPtr->nextNode->weight < newNode->weight)) {
                    cPtr = cPtr->nextNode;
                }

                insertToQueue(self, cPtr, newNode, (bool)(cPtr->weight < newNode->weight));
            } else {
                self->head = newNode;
            }
        } else {
            newNode->nextNode = NULL;
            self->head = self->tail = newNode;
            self->root = newNode;
        }
    }

    return true;
}

//assumes little endian
void 
printBits (uint32_t const size, 
           void const * const ptr) 
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;

    for (i=size-1; i >= 0; i--) {
        for (j=7; j >= 0; j--) {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
}

static uint32_t 
reverse_bits (uint32_t code, 
              uint32_t codeLength) 
{
    uint32_t result = code; // result will be reversed bits of code; first get LSB of code
    uint32_t size = sizeof(code) * 8 - 1; // extra shift needed at end

    for (code >>= 1; code; code >>= 1) {   
        result <<= 1;
        result |= code & 1;
        size--;
    }

    result <<= size; // shift when code's highest bits are zero
    result >>= sizeof(code) * 8 - codeLength;

    return result;
}

static inline void 
freeNode (qtreeNode* root,
          uint8_t depth,
          uint32_t code,
          ARCH* self)
{
    if (root->lchild == NULL && root->rchild == NULL)
    {
        free(root);
        root = NULL;
    }
}

static inline void 
checkNodeForSymbol (qtreeNode* root,
                    uint8_t depth,
                    uint32_t code,
                    ARCH* self)
{
    uint32_t reversedCode;

    if (root->symb) {
        reversedCode = reverse_bits(code, depth);
        self->codes[root->symb] = (codeInfo){root->symb, depth, reversedCode};
    }
}

static void
traverseTree (qtreeNode* root,
              void (*visitNode)(qtreeNode*, uint8_t, uint32_t, ARCH*),
              uint8_t depth, 
              uint32_t code, 
              ARCH* self) 
{
    if (root) {
        visitNode(root, depth, code, self);
        
        code = code << 1;
        traverseTree(root->lchild, visitNode, depth + 1, code | 0, self);
        traverseTree(root->rchild, visitNode, depth + 1, code | 1, self);
    }
}

static bool 
generateCodeTable (ARCH* self) 
{
    codeInfo *codeTable = self->codes;
    traverseTree(self->root, checkNodeForSymbol, (uint8_t)0, (uint32_t)0, self);

    for (int i = 0; i < 256; ++i) {
        if ((codeTable[i].length) > 0) {
            (self->numberOfCodes)++;
        }
    }

    return 0;
}

static bool 
writeCodesToFile (ARCH* self, 
                  const char* dstFileName) 
{
    codeInfo codes[self->numberOfCodes];
    codeInfo *codeTable = self->codes;

    FILE *dstFile = fopen(dstFileName, "w+");

    for (int i = 0, j = 0; i < 256; ++i) {
        if ((codeTable[i].length) > 0) {
            codes[j++] = codeTable[i];
        }
    }

    self->archInfo.tableLength = self->numberOfCodes;
    fseek(dstFile, sizeof(archiveInfo), SEEK_SET);    
    fwrite(codes, sizeof(codeInfo), self->numberOfCodes, dstFile);

    fclose(dstFile);

    return true;
}

static bool 
writeDataToFile (ARCH* self, 
                 const char* dstFileName, 
                 const char* srcFileName) 
{

    FILE *dstFile = fopen(dstFileName, "a+");
    FILE *srcFile = fopen(srcFileName, "r");

    codeInfo *codeTable = self->codes;
    uint8_t readBuff[BUFFER_SIZE];
    uint8_t nextReadedCharAsciiCode;
    uint8_t nextReadedCharCodeLength;

    uint32_t writeBuff[BUFFER_SIZE] = {0};
    uint32_t readedChars;
    uint32_t tempCode_1 = 0, tempCode_2 = 0;
    uint32_t writedBlocks = 0;
    uint32_t i;

    int32_t freeBlocks = BUFFER_SIZE, freeBits = BITS_IN_BLOCK;
    int32_t availableBits = 0;

    while ((freeBlocks) > 0) {
        if ((bool)(readedChars = fread(readBuff, sizeof(uint8_t), BUFFER_SIZE, srcFile))) {

            for (i = 0; i < readedChars; i++) {
                nextReadedCharAsciiCode = readBuff[i];
                nextReadedCharCodeLength = codeTable[nextReadedCharAsciiCode].length;

                availableBits = freeBits - nextReadedCharCodeLength;

                if (availableBits > 0) {
                    tempCode_1 = codeTable[nextReadedCharAsciiCode].code;
                    tempCode_1 <<= (BITS_IN_BLOCK - freeBits);
                    //write code of another readed character
                    writeBuff[BUFFER_SIZE - freeBlocks] |= tempCode_1;

                    freeBits -= nextReadedCharCodeLength;
                } else {
                    tempCode_1 = (codeTable[nextReadedCharAsciiCode].code << (BITS_IN_BLOCK - nextReadedCharCodeLength + abs(availableBits)));
                    tempCode_2 = (codeTable[nextReadedCharAsciiCode].code >> freeBits);
                    freeBits = BITS_IN_BLOCK + availableBits;
                    writeBuff[BUFFER_SIZE - freeBlocks] |= tempCode_1;
                    freeBlocks--;
               
                    if (freeBlocks == 0) {
                        fwrite(writeBuff, sizeof(uint32_t), BUFFER_SIZE, dstFile);
                        writedBlocks++;
                        memset(writeBuff, 0, sizeof(writeBuff));
                        
                        freeBlocks = BUFFER_SIZE;
                        freeBits = BITS_IN_BLOCK;
                
                        if (i >= readedChars) 
                            break;
                        
                        i--;
                    } else {
                        writeBuff[BUFFER_SIZE - freeBlocks] |= tempCode_2;
                    }
                }
            }
        } else {
            if (freeBlocks - 1 < BUFFER_SIZE) {
                fwrite(writeBuff, sizeof(uint32_t), BUFFER_SIZE - freeBlocks + 1, dstFile);
                writedBlocks++;
            }

            self->archInfo.remainingBits = (sizeof(writeBuff) * 8) - (((freeBlocks - 1) * BITS_IN_BLOCK) + freeBits);
            self->archInfo.numberOfBlocks = writedBlocks;
            break;
        }
    }

    fclose(srcFile);
    fclose(dstFile);

    return true;
}

static bool 
writeArchiveInfo(ARCH* self, 
                 const char* dstFileName) 
{
    FILE* dstFile = fopen(dstFileName, "r+");

    fseek(dstFile, 0, SEEK_SET);
    fwrite(&(self->archInfo), sizeof(archiveInfo), 1, dstFile);

    fclose(dstFile);

    return true;
}

bool 
compress (ARCH* self, 
          const char* dstFileName, 
          const char* srcFileName) 
{
    buildQueue(self, srcFileName);
    buildTree(self);
    generateCodeTable(self);
    writeCodesToFile(self, dstFileName);
    writeDataToFile(self, dstFileName, srcFileName);
    writeArchiveInfo(self, dstFileName);
    return true;
}

bool 
decompress (ARCH* self, 
            const char* dstFileName, 
            const char* srcFileName)
{
    FILE *dstFile = fopen(dstFileName, "w+");
    FILE *srcFile = fopen(srcFileName, "r");

    readArchiveInfo(self, srcFile);
    rebuildTree(self, srcFile);
    fileDecode(self, dstFile, srcFile);

    fclose(dstFile);
    fclose(srcFile);

    return true;
}

static bool 
readArchiveInfo (ARCH* self, 
                 FILE* srcFile)
{
    if(fread(&(self->archInfo), sizeof(archiveInfo), 1, srcFile)) {
        return true;
    } else {
        return false;
    }
}

static bool 
fileDecode (ARCH* self, 
                  FILE* dstFile, 
                  FILE* srcFile) 
{
    register uint32_t currentBitMask = 1;
    
    int32_t  remainingSignificantBits = self->archInfo.remainingBits;
    uint8_t  writeBuff[BUFFER_SIZE * 16] = {0};
    uint32_t readBuff[BUFFER_SIZE] = {0};
    uint32_t currentWriteBuffByte = 0;
    uint32_t readedBlocksNumber = 0;
    uint32_t totalReadedBlocks = 0;
    uint32_t currentBlock = 0;
    bool     isLastBlock = false;

    qtreeNode* cNodePtr = self->root;

    while ((bool)(readedBlocksNumber = fread(readBuff, sizeof(uint32_t), BUFFER_SIZE, srcFile))) {
        if (++totalReadedBlocks == self->archInfo.numberOfBlocks) {
            isLastBlock = true;
        }

        cNodePtr = self->root;

        for (currentBlock = 0; currentBlock < readedBlocksNumber; ++currentBlock) {
            currentBitMask = 1;
            
            while (currentBitMask) {

                if (cNodePtr->symb != 0) {
                    writeBuff[currentWriteBuffByte] = cNodePtr->symb;
                    currentWriteBuffByte++; 

                    cNodePtr = self->root;

                    if (currentWriteBuffByte == (BUFFER_SIZE * 16)) {
                        fwrite(writeBuff, sizeof(uint8_t), BUFFER_SIZE * 16, dstFile);
                        memset(writeBuff, 0, sizeof(writeBuff));
                        currentWriteBuffByte = 0;
                    }
                }

                cNodePtr = (readBuff[currentBlock] & currentBitMask) ? cNodePtr->rchild : cNodePtr->lchild;

                currentBitMask <<= 1;

                if (isLastBlock) {
                    remainingSignificantBits--;
                    if (remainingSignificantBits < 0) {
                        goto finish;
                    }
                }
            }
        }

        memset(readBuff, 0, sizeof(readBuff));
    }

finish:

    if(currentWriteBuffByte > 0) {
        fwrite(writeBuff, sizeof(uint8_t), currentWriteBuffByte, dstFile);
    }

    return true;
}

static void 
rebuildNodes (qtreeNode* root, 
              uint8_t length, 
              uint32_t code, 
              uint8_t symb) 
{
    qtreeNode *newNode = initQTreeNode();

    if(length > 0) {
        if (code & 1) {
            if (root->rchild == NULL) 
                root->rchild = newNode;

            rebuildNodes(root->rchild, length - 1, code >> 1, symb);
        } else {
            if (root->lchild == NULL) 
                root->lchild = newNode;

            rebuildNodes(root->lchild, length - 1, code >> 1, symb);
        }
    } else {
        root->symb = symb;
    }
}

static bool 
rebuildTree (ARCH* self, 
             FILE *srcFile) 
{
    uint8_t numberOfCodes = self->archInfo.tableLength;
    uint8_t readedCodesNum;
    codeInfo codes[numberOfCodes];

    if(!(bool)(readedCodesNum = fread(codes, sizeof(codeInfo), numberOfCodes, srcFile))) {
        return false;
    }

    self->root = initQTreeNode();
    
    for (int i = numberOfCodes - 1; i >= 0; --i) {
        rebuildNodes(self->root, codes[i].length, codes[i].code, codes[i].character);
    }

    return true;
}

static qtreeNode* 
initQTreeNode (void) 
{
    qtreeNode *newElement = (qtreeNode*) malloc(sizeof(qtreeNode));
 
    newElement->rchild = NULL;
    newElement->lchild = NULL;
    newElement->nextNode = NULL;
    newElement->symb = 0;

    return newElement;
}

ARCH* 
initArch (void) 
{
    ARCH *self = (ARCH*) calloc(1, sizeof(ARCH));
    self->numberOfCodes = 0;
    self->head = NULL;
    self->tail = NULL;
    self->root = NULL;

    return self;
}