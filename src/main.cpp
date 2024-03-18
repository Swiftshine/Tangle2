#include "tangle.h"

void printUsage() {
    printf("Usage: tangle <usage> <input> <output> <compression type>\n");
    printf("Usages:\n");
    printf("pack    - pack a gfa file without compression\n");
    printf("archive - pack a gfa file with compression\n");
    printf("unpack  - unpack a gfa file\n");
    printf("Compression types:\n");
    printf("bpe     - Byte Pair Encoding. Used for Kirby's Epic Yarn and Yoshi's Wooly World\n");
    printf("lz77    - LZ77. Used for Yoshi and Poochy's Wooly World and Kirby's Extra Epic Yarn.\n");
}

int main(int argc, char* argv[]) {
    if (argc < ARG_COMPRESSION_TYPE || argc >= TOTAL_ARGS) {
        printUsage();
        return 1;
    }

    std::string usage   = argv[ARG_USAGE];
    std::string input   = argv[ARG_INPUT_NAME];
    std::string output  = argv[ARG_OUTPUT_NAME];
    
    if (usage != "pack" && usage != "archive" && usage != "unpack") {
        printUsage();
        return 2;
    }

    if (usage == "pack") {
        GFA::pack(input, output);
        return 0;
    }
    else if (usage == "archive") {
        // placeholder compression type
        GFA::archive(input, output, 1);
        return 0;
    }
    else if (usage == "unpack") {
        GFA::unpack(input, output);
        return 0;
    }

    printf("You shouldn't be here.\n");
    return 3;
}