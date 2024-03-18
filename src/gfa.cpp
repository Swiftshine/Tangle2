#include "tangle.h"
namespace fs = std::filesystem;

template <typename T>
void appendData(std::vector<char>& vec, const T& data) {
    const char* bytes = reinterpret_cast<const char*>(&data);
    vec.insert(vec.end(), bytes, bytes + sizeof(data));
}


u32 GFA::getHash(std::string filename) {
    u32 result = 0;
    int len = filename.length();
    char curChar;
    for (int i = 0; i < len; i++) {
        curChar = filename[i];
        if (curChar == '\0') break;

        result = curChar + (result * 0x89);
    }
    return result;
}

void GFA::pack(std::string input, std::string output) {
    if (input.empty() || output.empty()) {
        printf("Error - input or output names are empty\n");
        return;
    }

    
    std::vector<char> output_data; // this is what we'll put the contents of the archive in

    // get all filenames and number of files

    u32 num_files = 0;

    std::vector<std::string> filenames;
    std::vector<char> packed_data;
    std::vector<u32> file_sizes;
    for (auto const& dir_entry : fs::directory_iterator{input}) {
        std::string filename = dir_entry.path().filename().string();
        std::fstream file(dir_entry.path().string(), std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            printf("Warning - file %s could not be opened, skipping\n", filename.c_str());
            continue;
        }
        file.seekg(0, std::ios::end);
        u32 filesize = file.tellg();

        file_sizes.push_back(filesize);
        file.seekg(0, std::ios::beg);
        
        std::vector<char> file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        packed_data.insert(packed_data.end(), file_contents.begin(), file_contents.end());
        file.close();
        filenames.push_back(filename + '\0');
        num_files++;
	}

    if (num_files < 1) {
        printf("Error - there are no files in folder %s\n", input.c_str());
        return;
    }

    if (packed_data.size() < 1) {
        printf("Error - files are empty\n");
        return;
    }

    u32 name_length_totals = 0;
    for (int i = 0; i < num_files; i++) {
        name_length_totals += filenames[i].length();
    }


    // set up archive header
    // currently only set up for KEY Wii. replace this value if you need it for another game

    GFA::ArchiveHeader archiveHeader;
    archiveHeader.magic[0] = 'G';
    archiveHeader.magic[1] = 'F';
    archiveHeader.magic[2] = 'A';
    archiveHeader.magic[3] = 'C';
    archiveHeader._4 = Tangle::swapEndianness32(0x00030000);
    archiveHeader.version = 1;
    archiveHeader.fileCountOffset = 0x2C;
    archiveHeader.fileInfoSize = sizeof(archiveHeader.fileCount) + (sizeof(GFA::FileEntry) * num_files) + name_length_totals;
    archiveHeader.dataOffset = sizeof(GFA::ArchiveHeader) + archiveHeader.fileInfoSize;
    if (archiveHeader.dataOffset % 0x10 != 0) {
        u32 padding = 0x10 - (archiveHeader.dataOffset % 0x10);
        archiveHeader.dataOffset += padding;
    }
    archiveHeader.dataSize = sizeof(GFA::CompressionHeader) + packed_data.size();
    u32 num_padding = 0;
    for (int i = 0; i < 0x10; i++) {
        archiveHeader.padding[i] = (u8)(0);
        num_padding++;
    }
    archiveHeader.fileCount = num_files;
    
    // done with archive header
    appendData(output_data, archiveHeader);

    u32 data_offset_totals = archiveHeader.dataOffset + sizeof(GFA::CompressionHeader);
    u32 name_offset_totals = sizeof(GFA::ArchiveHeader) + (sizeof(GFA::FileEntry) * num_files);
    for (int i = 0; i < num_files; i++) {
        GFA::FileEntry entry;
        entry.hash = GFA::getHash(filenames[i]);
        entry.nameOffset = name_offset_totals;
        name_offset_totals += filenames[i].length();
        entry.decompressedSize = file_sizes[i];
        entry.dataOffset = data_offset_totals;
        data_offset_totals += file_sizes[i];

        appendData(output_data, entry);
    }

    // append strings
    for (const auto& string : filenames) {
        output_data.insert(output_data.end(), string.begin(), string.end());
    }

    // append padding if appropriate
    while (output_data.size() % 0x10 != 0) {
        char c = 0;
        appendData(output_data, c);
    }
    
    // setup compression header
    GFA::CompressionHeader compressionHeader;
    compressionHeader.magic[0] = 'G';
    compressionHeader.magic[1] = 'F';
    compressionHeader.magic[2] = 'C';
    compressionHeader.magic[3] = 'P';
    compressionHeader.version = 1;
    compressionHeader.compressionType = CT_NONE;
    compressionHeader.decompressedSize = packed_data.size();
    compressionHeader.compressedSize = packed_data.size();

    while (output_data.size() < archiveHeader.dataOffset) {
        char c = 0;
        appendData(output_data, c);
    }
    // done with compression header
    appendData(output_data, compressionHeader);

    // append packed data
    output_data.insert(output_data.end(), packed_data.begin(), packed_data.end());
    std::fstream out(output, std::ios::out | std::ios::binary);
    if (!out.is_open()) {
        printf("Error - could not create or open file %s\n", output.c_str());
        return;
    }

    out.write(output_data.data(), output_data.size());
    out.close();
    printf("Finished packing folder %s into %s\n", input.c_str(), output.c_str());
}

void GFA::unpack(std::string input, std::string output) {

    printf("Finished unpacking %s into folder %s\n", input.c_str(), output.c_str());
}

void GFA::archive(std::string input, std::string output, int compressionType) {
    if (input.empty()  || output.empty()) {
        printf("Error - inpt or output names are empty\n");
        return;
    }

    std::vector<char> output_data;
 
    std::vector<char> decompressed_data;
    std::vector<std::string> filenames;
    std::vector<u32> file_sizes;

    u32 num_files = 0;
    u32 size_all_files = 0;
    for (auto const& dir_entry : fs::directory_iterator{input}) {

        // open file solely to get the filesize
        std::string filename = dir_entry.path().filename().string();
        std::fstream file_uncompressed(dir_entry.path().string(), std::ios::in | std::ios::binary);
        if (!file_uncompressed.is_open()) {
            printf("Warning - file %s could not be opened, skipping\n", filename.c_str());
            continue;
        }
        file_uncompressed.seekg(0, std::ios::end);
        u32 filesize = file_uncompressed.tellg();
        file_uncompressed.seekg(0, std::ios::beg);
        std::vector<char> file_contents;
        file_contents.assign((std::istreambuf_iterator<char>(file_uncompressed)), std::istreambuf_iterator<char>());
        decompressed_data.insert(decompressed_data.end(), file_contents.begin(), file_contents.end());
        file_uncompressed.close();
        

        size_all_files += filesize;
        file_sizes.push_back(filesize);
        filenames.push_back(filename + '\0');

        num_files++;
    }
    // write all PACKED data
    std::fstream packed_files("temp1.bin", std::ios::out | std::ios::binary);
    packed_files.write(decompressed_data.data(), decompressed_data.size());
    packed_files.close();

    // compress data
    // these functions only work with c-style file streams atm :(
    FILE* temp1 = fopen("temp1.bin", "rb");
    if (!temp1) {
        printf("error - i'll rewrite this later\n");
        return;
    }
    FILE* temp2 = fopen("temp2.bin", "wb");
    if (!temp2) {
        fclose(temp1);
        printf("Error - couldn't create temp file\n");
        return;
    }
    if (compressionType == CT_BPE)
        BPE::compress(temp1, temp2);
    
    fclose(temp1);
    fs::remove("temp1.bin");
    fclose(temp2);
    std::fstream compressed_files("temp2.bin", std::ios::in | std::ios::binary);
    std::vector<char> compressed_data((std::istreambuf_iterator<char>(compressed_files)), std::istreambuf_iterator<char>());
    compressed_files.close();
    fs::remove("temp2.bin");

    if (num_files < 1) {
        printf("Error - there are no files in folder %s\n", input.c_str());
        return;
    }

    if (compressed_data.size() < 1) {
        printf("Error - files are empty\n");
        return;
    }

    u32 name_length_totals = 0;
    for (int i = 0; i < num_files; i++) {
        name_length_totals += filenames[i].length();
    }

    // the same as packing mostly

    GFA::ArchiveHeader archiveHeader;
    archiveHeader.magic[0] = 'G';
    archiveHeader.magic[1] = 'F';
    archiveHeader.magic[2] = 'A';
    archiveHeader.magic[3] = 'C';
    archiveHeader._4 = Tangle::swapEndianness32(0x00030000);
    archiveHeader.version = 1;
    archiveHeader.fileCountOffset = 0x2C;
    archiveHeader.fileInfoSize = sizeof(archiveHeader.fileCount) + (sizeof(GFA::FileEntry) * num_files) + name_length_totals;
    archiveHeader.dataOffset = sizeof(GFA::ArchiveHeader) + archiveHeader.fileInfoSize;
    if (archiveHeader.dataOffset % 0x10 != 0) {
        u32 padding = 0x10 - (archiveHeader.dataOffset % 0x10);
        archiveHeader.dataOffset += padding;
    }
    archiveHeader.dataSize = sizeof(GFA::CompressionHeader) + compressed_data.size();
    u32 num_padding = 0;
    for (int i = 0; i < 0x10; i++) {
        archiveHeader.padding[i] = (u8)(0);
        num_padding++;
    }
    archiveHeader.fileCount = num_files;
    
    // done with archive header
    appendData(output_data, archiveHeader);


    u32 data_offset_totals = archiveHeader.dataOffset + sizeof(GFA::CompressionHeader);
    u32 name_offset_totals = sizeof(GFA::ArchiveHeader) + (sizeof(GFA::FileEntry) * num_files);
    for (int i = 0; i < num_files; i++) {
        GFA::FileEntry entry;
        entry.hash = GFA::getHash(filenames[i]);
        entry.nameOffset = name_offset_totals;
        name_offset_totals += filenames[i].length();
        entry.decompressedSize = file_sizes[i];
        entry.dataOffset = data_offset_totals;
        data_offset_totals += file_sizes[i];

        appendData(output_data, entry);
    }

    // append strings
    for (const auto& string : filenames) {
        output_data.insert(output_data.end(), string.begin(), string.end());
    }

    // append padding if appropriate
    while (output_data.size() % 0x10 != 0) {
        char c = 0;
        appendData(output_data, c);
    }

        // setup compression header
    GFA::CompressionHeader compressionHeader;
    compressionHeader.magic[0] = 'G';
    compressionHeader.magic[1] = 'F';
    compressionHeader.magic[2] = 'C';
    compressionHeader.magic[3] = 'P';
    compressionHeader.version = 1;
    compressionHeader.compressionType = compressionType;
    compressionHeader.decompressedSize = size_all_files;
    compressionHeader.compressedSize = compressed_data.size();

    while (output_data.size() < archiveHeader.dataOffset) {
        char c = 0;
        appendData(output_data, c);
    }
    // done with compression header
    appendData(output_data, compressionHeader);

    // append packed data
    output_data.insert(output_data.end(), compressed_data.begin(), compressed_data.end());
    std::fstream out(output, std::ios::out | std::ios::binary);
    if (!out.is_open()) {
        printf("Error - could not create or open file %s\n", output.c_str());
        return;
    }

    out.write(output_data.data(), output_data.size());
    out.close();
    printf("Finished archiving folder %s into %s\n", input.c_str(), output.c_str());
}