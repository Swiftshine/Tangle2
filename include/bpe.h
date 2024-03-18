#pragma once
void expand(FILE *input, FILE *output);

namespace BPE {
    void decompress(std::fstream&, std::fstream&);
    //void decompress(FileReader, FileReader);
    //static void decompress(FILE* input, FILE* output);
    void compress(FILE* input, FILE* output);


} // namespace BPE