#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include "argh.h"

namespace fs = std::filesystem;

#ifndef EXIT_SUCCESS
    #define	EXIT_SUCCESS	0
#endif
#ifndef EXIT_FAILURE
    #define	EXIT_FAILURE	1
#endif

static int print_usage() {
    printf("-split\t\tThe opposite of \"-unsplit\". Required to split files.\n\n");
    printf("-unsplit\tThe opposite of \"-split\". Required to join files.\n\n");

    printf("-filename\tRequired if splitting. Specifies the file to be split.\n\n");
    printf("-foldername\tRequired if unsplitting. Specifies the folder whose contents will be unsplit.\n\n");
    printf("-size\t\tRequired if splitting. Specifies the target size (in bytes) the parts of the file.\n\n");
    printf("-suffix\t\tOptional. Specifies the suffix of each file. The default is \"_part\"[number]; e.g. MyFile_part1.bin\n\n");
    printf("-extension\tOptional. Can be used to specify *if* the split files will have an extension, and *what* it will be.\n\t\tIf the flag is present but no extension is specified, the default is \".bin\".\n\t\tIf unsplitting a file, this field is ALWAYS ignored.\n\n");
    return EXIT_FAILURE;
}

static int unsplit_file(std::string_view folder_name, std::string_view suffix, std::string_view output_filename);
static int split_file(std::string_view filename, unsigned int byte_limit, std::string_view suffix, std::string_view extension);

enum Usage {
    Split = 0,
    Unsplit = 1,
};


int main(int argc, char* argv[]) {
    argh::parser cmd;
    cmd.add_params({"-filename", "-suffix", "-size", "-extension"});
    cmd.parse(argv);

    if ((!cmd["-split"] && !cmd["-unsplit"]) || (cmd["-split"] && cmd["-unsplit"])) {
        printf("Need to use exactly one usage argument.\n");
        return print_usage();
    }

    int usage = cmd["split"] ? Usage::Split : Usage::Unsplit;

    std::string folder_name;
    std::string suffix;


    cmd("-foldername") >> folder_name;
    
    cmd("-suffix") >> suffix;

    std::string filename;
    cmd("-filename") >> filename;

    if (suffix.empty()) { suffix = "_part"; }

    if (Usage::Unsplit == usage) {
        if (filename.empty()) {
            filename = folder_name + " - unsplit";
        }
        return unsplit_file(folder_name, suffix, filename);
    }

    // if you're still here, you're splitting.
    // there's a bit more setup to go through.

    if (filename.empty()) {
        printf("Need to specify filename.\n");
        return print_usage();
    }

    std::string extension;
    int         size_signed = -1;

    {
        std::string temp;
        cmd("-size") >> temp;
        if (temp.empty()) {
            printf("Need to specify size limit.\n");
            return print_usage();
        }
        size_signed = std::stoi(temp);
    }

    cmd("-extension") >> extension;

    if (extension.empty() && cmd["extension"]) { extension = ".bin"; }

    if (size_signed < 1) {
        printf("Size cannot be less than 1 byte. Given size was %d byte(s).\n\n", size_signed);
        return EXIT_FAILURE;
    } else if (size_signed < 1000) {
        printf("Splitting a file into sizes less than 1,000 bytes is impractical. The file was not split.\n");
        return EXIT_FAILURE;
    }

    unsigned int size = static_cast<unsigned int>(size_signed);

    return split_file(filename, size, suffix, extension);
} 

int unsplit_file(std::string_view folder_name, std::string_view suffix, std::string_view output_filename) {
    // determine folder path
    fs::path folder_path = folder_name.empty() ? fs::current_path() : fs::path(folder_name);


    std::fstream output_file(output_filename.data(), std::ios::out | std::ios::binary);
    if (!output_file.is_open()) {
        printf("Failed to create or open file %s.\n", output_filename.data());
        return EXIT_FAILURE;
    }

    if (!fs::is_directory(folder_path)) {
        printf("Folder %s does not exist.\n", folder_path.c_str());
        return EXIT_FAILURE;
    }

    // collect all matching file paths
    std::vector<fs::path> file_paths;
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && entry.path().filename().string().find(suffix.data()) != std::string::npos) {
            file_paths.push_back(entry.path());
        }
    }

    // sort files to maintain order (if necessary)
    std::sort(file_paths.begin(), file_paths.end());

    if (file_paths.empty()) {
        printf("No files found with suffix %s in folder %s.\n", suffix.data(), folder_path.c_str());
        return EXIT_FAILURE;
    }

    // iterate over files and write their contents to the output file
    for (const auto& path : file_paths) {
        std::ifstream input_file(path, std::ios::in | std::ios::binary);
        if (!input_file.is_open()) {
            printf("Failed to open file %s.\n", path.c_str());
            return EXIT_FAILURE;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
        output_file.write(buffer.data(), buffer.size());

        input_file.close();
    }

    output_file.close();

    printf("Successfully combined files into %s.\n", output_filename.data());
    return EXIT_SUCCESS;
}


int split_file(std::string_view filename, unsigned int byte_limit, std::string_view suffix, std::string_view extension) {

    fs::path file_path(filename);
    std::string base_name = file_path.stem().string(); // remove extension
    std::string file_extension = file_path.extension().string();  // extract extension

    std::fstream file(filename.data(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        printf("File %s does not exist.\n", filename.data());
        return EXIT_FAILURE;
    }

    std::vector<char> full_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    unsigned int total_size = full_contents.size();
    // calculate number of splits
    unsigned int num_splits = (total_size + byte_limit - 1) / byte_limit;

    // determine if an output folder is warranted
    bool use_output_folder = (num_splits > 10);
    std::string folder_name = "output";

    if (use_output_folder) {
        if (!fs::exists(folder_name)) {
            if (!fs::create_directory(folder_name)) {
                printf("Failed to create directory %s.\n", folder_name.c_str());
                return EXIT_FAILURE;
            }
        }
    }

    unsigned int remaining = total_size;
    unsigned int offset = 0;
    unsigned int part = 0;

    while (remaining > 0) {
        unsigned int chunk_size = std::min(byte_limit, remaining);
        std::string output_filename = (use_output_folder ? folder_name + "/" : "") +
                                      base_name + std::string(suffix) + std::to_string(part) + ((extension.find(".") != std::string::npos) ? std::string(extension) : std::string(".") + std::string(extension));
        std::fstream output_file(output_filename, std::ios::out | std::ios::binary);
        if (!output_file.is_open()) {
            printf("Failed to create file %s.\n", output_filename.c_str());
            return EXIT_FAILURE;
        }

        output_file.write(full_contents.data() + offset, chunk_size);
        output_file.close();

        offset += chunk_size;
        remaining -= chunk_size;
        part++;
    }

    printf("Successfully split file %s.\n", filename.data());
    return EXIT_SUCCESS;
}
