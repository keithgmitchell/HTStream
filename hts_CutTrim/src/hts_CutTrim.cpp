#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <vector>
#include <fstream>
#include "ioHandler.h"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <map>
#include <unordered_map>
#include <boost/functional/hash.hpp>

#include "hts_CutTrim.h"

namespace
{
    const size_t SUCCESS = 0;
    const size_t ERROR_IN_COMMAND_LINE = 1;
    const size_t ERROR_UNHANDLED_EXCEPTION = 2;

} // namespace

namespace bi = boost::iostreams;

int main(int argc, char** argv)
{
    const std::string program_name = "hts_CutTrim";
    std::string app_description =
                       "The hts_CutTrim application trims a set number of bases from the 5'\n";
    app_description += "  and/or 3' end of each read\n";

    try
    {
        /** Define and parse the program options
         */
        namespace po = boost::program_options;
        po::options_description standard = setStandardOptions();
            // version|v ; help|h ; notes|N ; stats-file|L ; append-stats-file|A
        po::options_description input = setInputOptions();
            // read1-input|1 ; read2-input|2 ; singleend-input|U
            // tab-input|T ; interleaved-input|I
        po::options_description output = setOutputOptions(program_name);
          // force|F ; uncompressed|u ; fastq-output|f
          // tab-output|t ; interleaved-output|i ; unmapped-output|z

        po::options_description desc("Application Specific Options");

        setDefaultParamsCutting(desc);
            // no-orphans|n ; stranded|s ; min-length|m

        desc.add_options()
            ("r1-cut-left,a", po::value<size_t>()->default_value(0)->notifier(boost::bind(&check_range<size_t>, "r1-cut-left", _1, 0, 10000)), "Cut length of sequence from read 1 left (5') end (min 0, max 10000)");
        desc.add_options()
            ("r1-cut-right,b", po::value<size_t>()->default_value(0)->notifier(boost::bind(&check_range<size_t>, "r1-cut-right", _1, 0, 10000)), "Cut length of sequence from read 1 right (3') end (min 0, max 10000)");
        desc.add_options()
            ("r2-cut-left,c", po::value<size_t>()->default_value(0)->notifier(boost::bind(&check_range<size_t>, "r2-cut-left", _1, 0, 10000)), "Cut length of sequence from read 2 left (5') end (min 0, max 10000)");
        desc.add_options()
            ("r2-cut-right,d", po::value<size_t>()->default_value(0)->notifier(boost::bind(&check_range<size_t>, "r2-cut-right", _1, 0, 10000)), "Cut length of sequence from read 2 right (3') end (min 0, max 10000)");
        desc.add_options()
            ("max-length,M", po::value<size_t>()->default_value(0)->notifier(boost::bind(&check_range<size_t>, "max-length", _1, 0, 10000)), "Maximum allowed length of read, effectively right trims to max-length (min 0, max 10000)");

        po::options_description cmdline_options;
        cmdline_options.add(standard).add(input).add(output).add(desc);

        po::variables_map vm;

        try
        {
            po::store(po::parse_command_line(argc, argv, cmdline_options), vm); // can throw

            /** --help option
            */
            version_or_help(program_name, app_description, cmdline_options, vm);
            po::notify(vm); // throws on error, so do after help in case

            std::shared_ptr<OutputWriter> pe = nullptr;
            std::shared_ptr<OutputWriter> se = nullptr;
            outputWriters(pe, se, vm);

            std::string statsFile(vm["stats-file"].as<std::string>());
            TrimmingCounters counters(statsFile, vm["force"].as<bool>(), vm["append-stats-file"].as<bool>(), program_name, vm["notes"].as<std::string>());

            if(vm.count("read1-input")) {
                if (!vm.count("read2-input")) {
                    throw std::runtime_error("must specify both read1 and read2 input files.");
                }
                auto read1_files = vm["read1-input"].as<std::vector<std::string> >();
                auto read2_files = vm["read2-input"].as<std::vector<std::string> >();
                if (read1_files.size() != read2_files.size()) {
                    throw std::runtime_error("must have same number of input files for read1 and read2");
                }
                for(size_t i = 0; i < read1_files.size(); ++i) {
                    bi::stream<bi::file_descriptor_source> is1{check_open_r(read1_files[i]), bi::close_handle};
                    bi::stream<bi::file_descriptor_source> is2{check_open_r(read2_files[i]), bi::close_handle};
                    InputReader<PairedEndRead, PairedEndReadFastqImpl> ifp(is1, is2);
                    helper_trim(ifp, pe, se, counters, vm["min-length"].as<size_t>() , vm["stranded"].as<bool>(), vm["no-orphans"].as<bool>(), vm["r1-cut-left"].as<size_t>(), vm["r1-cut-right"].as<size_t>(), vm["r2-cut-left"].as<size_t>(), vm["r2-cut-right"].as<size_t>(), vm["max-length"].as<size_t>() );
                }
            }
            if (vm.count("interleaved-input")) {
                auto read_files = vm["interleaved-input"].as<std::vector<std::string > >();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> inter{ check_open_r(file), bi::close_handle};
                    InputReader<PairedEndRead, InterReadImpl> ifi(inter);
                    helper_trim(ifi, pe, se, counters, vm["min-length"].as<size_t>() , vm["stranded"].as<bool>(), vm["no-orphans"].as<bool>(), vm["r1-cut-left"].as<size_t>(), vm["r1-cut-right"].as<size_t>(), vm["r2-cut-left"].as<size_t>(), vm["r2-cut-right"].as<size_t>(), vm["max-length"].as<size_t>() );
                }
            }
            if(vm.count("singleend-input")) {
                auto read_files = vm["singleend-input"].as<std::vector<std::string> >();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> sef{ check_open_r(file), bi::close_handle};
                    InputReader<SingleEndRead, SingleEndReadFastqImpl> ifs(sef);
                    helper_trim(ifs, pe, se, counters, vm["min-length"].as<size_t>() , vm["stranded"].as<bool>(), vm["no-orphans"].as<bool>(), vm["r1-cut-left"].as<size_t>(), vm["r1-cut-right"].as<size_t>(), vm["r2-cut-left"].as<size_t>(), vm["r2-cut-right"].as<size_t>(), vm["max-length"].as<size_t>() );
                }
            }
            if(vm.count("tab-input")) {
                auto read_files = vm["tab-input"].as<std::vector<std::string> > ();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> tabin{ check_open_r(file), bi::close_handle};
                    InputReader<ReadBase, TabReadImpl> ift(tabin);
                    helper_trim(ift, pe, se, counters, vm["min-length"].as<size_t>() , vm["stranded"].as<bool>(), vm["no-orphans"].as<bool>(), vm["r1-cut-left"].as<size_t>(), vm["r1-cut-right"].as<size_t>(), vm["r2-cut-left"].as<size_t>(), vm["r2-cut-right"].as<size_t>(), vm["max-length"].as<size_t>() );
                }
            }
            if (!isatty(fileno(stdin))) {
                bi::stream<bi::file_descriptor_source> tabin {fileno(stdin), bi::close_handle};
                InputReader<ReadBase, TabReadImpl> ift(tabin);
                    helper_trim(ift, pe, se, counters, vm["min-length"].as<size_t>() , vm["stranded"].as<bool>(), vm["no-orphans"].as<bool>(), vm["r1-cut-left"].as<size_t>(), vm["r1-cut-right"].as<size_t>(), vm["r2-cut-left"].as<size_t>(), vm["r2-cut-right"].as<size_t>(), vm["max-length"].as<size_t>() );
            }
            counters.write_out();
        }
        catch(po::error& e)
        {
            std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
            return ERROR_IN_COMMAND_LINE;
        }

    }
    catch(std::exception& e)
    {
        std::cerr << "\n\tUnhandled Exception: "
                  << e.what() << std::endl;
        return ERROR_UNHANDLED_EXCEPTION;

    }
return SUCCESS;

}