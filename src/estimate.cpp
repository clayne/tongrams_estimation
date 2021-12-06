#include <chrono>
#include <iostream>

#include "../external/tongrams/external/cmd_line_parser/include/parser.hpp"

#include "configuration.hpp"
#include "estimation.hpp"

int main(int argc, char** argv) {
    using namespace tongrams;

    configuration config;
    cmd_line_parser::parser parser(argc, argv);
    parser.add("text_filename", "Input text filename.");
    parser.add("order", "Language model order. It must be > 2 and <= " +
                            std::to_string(global::max_order) + ".");
    parser.add("ram",
               "Amount to RAM dedicated to estimation in GiB. Default is " +
                   std::to_string(static_cast<uint64_t>(
                       static_cast<double>(config.RAM) / essentials::GiB)) +
                   " GiB.",
               "--ram", false);
    parser.add(
        "tmp_dir",
        "Temporary directory used for estimation. Default is directory '" +
            constants::default_tmp_dirname + "'.",
        "--tmp", false);
    parser.add("num_threads",
               "Number of threads. Default is " +
                   std::to_string(config.num_threads) + " on this machine.",
               "--thr", false);
    parser.add("compress_blocks",
               "Compress temporary files during estimation. Default is " +
                   (config.compress_blocks ? std::string("true")
                                           : std::string("false")) +
                   ".",
               "--compress_blocks", true);
    // parser.add("p",
    //            "Probability quantization bits.",
    //            "--p", false);
    // parser.add("b",
    //            "Backoff quantization bits.",
    //            "--b", false);
    parser.add("out",
               "Output filename. Default is '" +
                   constants::default_output_filename + "'.",
               "--out", false);
    if (!parser.parse()) return 1;

    config.text_filename = parser.get<std::string>("text_filename");
    if (!util::exists(config.text_filename.c_str())) {
        std::cerr << "Error: corpus file does not exist" << std::endl;
        return 1;
    }

    config.text_size = util::file_size(config.text_filename.c_str());
    std::cerr << "reading from '" << config.text_filename << "' ("
              << config.text_size << " bytes)" << std::endl;
    config.max_order = parser.get<uint64_t>("order");
    if (config.max_order <= 2 or config.max_order > global::max_order) {
        std::cerr << "invalid language model order" << std::endl;
        return 1;
    }

    size_t available_ram = sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);

    if (parser.parsed("ram")) {
        uint64_t ram =
            static_cast<uint64_t>(parser.get<double>("ram") * essentials::GiB);
        if (ram > available_ram) {
            std::cerr << "Warning: this machine has "
                      << available_ram / essentials::GiB << " GiB of RAM."
                      << std::endl;
            std::cerr << "Thus, using defalt amount of "
                      << config.RAM / essentials::GiB << " GiB" << std::endl;
        } else {
            config.RAM = ram;
        }
    }
    if (parser.parsed("tmp_dir")) {
        config.tmp_dirname = parser.get<std::string>("tmp_dir");
    }
    if (parser.parsed("num_threads")) {
        config.num_threads = parser.get<uint64_t>("num_threads");
        if (config.num_threads == 0) {
            std::cerr << "number of threads must be > 0" << std::endl;
            return 1;
        }
    }
    if (parser.parsed("compress_blocks")) {
        config.compress_blocks = parser.get<bool>("compress_blocks");
    }
    if (parser.parsed("out")) {
        config.output_filename = parser.get<std::string>("out");
    }

    config.vocab_tmp_subdirname = config.tmp_dirname + "/vocab";
    bool ok = essentials::create_directory(config.tmp_dirname) and
              essentials::create_directory(config.vocab_tmp_subdirname);
    if (not ok) return 1;

    std::cerr << "estimating with " << config.RAM << "/" << available_ram
              << " bytes of RAM"
              << " (" << config.RAM * 100.0 / available_ram << "\%)\n";

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    estimation e(config);
    e.run();
    e.print_stats();

    return 0;
}
