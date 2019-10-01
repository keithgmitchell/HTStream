#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <vector>
#include <fstream>
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

#include "hts_PolyATTrim.h"

namespace
{
    const size_t SUCCESS = 0;
    const size_t ERROR_IN_COMMAND_LINE = 1;
    const size_t ERROR_UNHANDLED_EXCEPTION = 2;

} // namespace

namespace bi = boost::iostreams;

int main(int argc, char** argv)
{

    const std::string program_name = "hts_PolyATTrim";
    std::string app_description = "hts_PolyATTrim trims poly A and T sequences from a read.\n";
    app_description += "  The algorithm is borrowed from Fig 2, Bonfert et al. doi: 2017 10.1371/journal.pone.0170914\n";
    app_description += "  A sliding window of <window-size> (=6) is shifted from either end of the read\n";
    app_description += "  (adjustable with --no-left and --no-right) until the <max-mismatch-errorDensity> is\n";
    app_description += "  exceeded. The read is then trimmed as long as the following criteria are met:\n";
    app_description += "\tat least <perfect-windows> (=1) were observed\n";
    app_description += "\tat least <min-trim> (=5) bp will be trimmed\n";
    app_description += "\tno more than <max-trim> (=30) bp will be trimmed\n";
    app_description += "  These settings may need to be adjusted depending on library type.";
    
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
        setDefaultParamsTrim(desc);
            // no-left|l ; no-right|r

        desc.add_options()
            ("skip_polyA,j", po::bool_switch()->default_value(false), "Skip check for polyA sequence")
            ("skip_polyT,k", po::bool_switch()->default_value(false), "Skip check for polyT sequence")
            ("window-size,w", po::value<size_t>()->default_value(6)->notifier(boost::bind(&check_range<size_t>, "window-size", _1, 1, 10000)),    "Window size in which to trim (min 1, max 10000)")
            ("max-mismatch-errorDensity,e", po::value<double>()->default_value(0.30)->notifier(boost::bind(&check_range<double>, "max-mismatch-errorDensity", _1, 0.0, 1.0)), "Max percent of mismatches allowed in overlapped section (min 0.0, max 1.0)")
            ("perfect-windows,c", po::value<size_t>()->default_value(1)->notifier(boost::bind(&check_range<size_t>, "perfect-windows", _1, 0, 10000)),    "Number perfect match windows needed before a match is reported  (min 1, max 10000)")
            ("min-trim,M", po::value<size_t>()->default_value(5)->notifier(boost::bind(&check_range<size_t>, "min-trim", _1, 1, 10000)), "Min base pairs trim for AT tail (min 1, max 10000)")
            ("max-trim,x", po::value<size_t>()->default_value(30)->notifier(boost::bind(&check_range<size_t>, "max-trim", _1, 0, 10000)), "Max size a polyAT can be (min 0, max 10000)");

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
                    helper_trim(ifp, pe, se, counters, vm["min-length"].as<std::size_t>(), vm["skip_polyA"].as<bool>(), vm["skip_polyT"].as<bool>(), vm["min-trim"].as<std::size_t>(), vm["window-size"].as<std::size_t>(), vm["perfect-windows"].as<std::size_t>(), vm["max-trim"].as<std::size_t>(), vm["max-mismatch-errorDensity"].as<double>(), vm["stranded"].as<bool>(), vm["no-left"].as<bool>(), vm["no-right"].as<bool>(), vm["no-orphans"].as<bool>() );
                }
            }
            if (vm.count("interleaved-input")) {
                auto read_files = vm["interleaved-input"].as<std::vector<std::string > >();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> inter{ check_open_r(file), bi::close_handle};
                    InputReader<PairedEndRead, InterReadImpl> ifi(inter);
                    helper_trim(ifi, pe, se, counters, vm["min-length"].as<std::size_t>(), vm["skip_polyA"].as<bool>(), vm["skip_polyT"].as<bool>(), vm["min-trim"].as<std::size_t>(), vm["window-size"].as<std::size_t>(), vm["perfect-windows"].as<std::size_t>(), vm["max-trim"].as<std::size_t>(), vm["max-mismatch-errorDensity"].as<double>(), vm["stranded"].as<bool>(), vm["no-left"].as<bool>(), vm["no-right"].as<bool>(), vm["no-orphans"].as<bool>() );
                }
            }
            if(vm.count("singleend-input")) {
                auto read_files = vm["singleend-input"].as<std::vector<std::string> >();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> sef{ check_open_r(file), bi::close_handle};
                    InputReader<SingleEndRead, SingleEndReadFastqImpl> ifs(sef);
                    helper_trim(ifs, pe, se, counters, vm["min-length"].as<std::size_t>(), vm["skip_polyA"].as<bool>(), vm["skip_polyT"].as<bool>(), vm["min-trim"].as<std::size_t>(), vm["window-size"].as<std::size_t>(), vm["perfect-windows"].as<std::size_t>(), vm["max-trim"].as<std::size_t>(), vm["max-mismatch-errorDensity"].as<double>(), vm["stranded"].as<bool>(), vm["no-left"].as<bool>(), vm["no-right"].as<bool>(), vm["no-orphans"].as<bool>() );
                }
            }
            if(vm.count("tab-input")) {
                auto read_files = vm["tab-input"].as<std::vector<std::string> > ();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> tabin{ check_open_r(file), bi::close_handle};
                    InputReader<ReadBase, TabReadImpl> ift(tabin);
                    helper_trim(ift, pe, se, counters, vm["min-length"].as<std::size_t>(), vm["skip_polyA"].as<bool>(), vm["skip_polyT"].as<bool>(), vm["min-trim"].as<std::size_t>(), vm["window-size"].as<std::size_t>(), vm["perfect-windows"].as<std::size_t>(), vm["max-trim"].as<std::size_t>(), vm["max-mismatch-errorDensity"].as<double>(), vm["stranded"].as<bool>(), vm["no-left"].as<bool>(), vm["no-right"].as<bool>(), vm["no-orphans"].as<bool>() );
                }
            }
            if (!isatty(fileno(stdin))) {
                bi::stream<bi::file_descriptor_source> tabin {fileno(stdin), bi::close_handle};
                InputReader<ReadBase, TabReadImpl> ift(tabin);
                helper_trim(ift, pe, se, counters, vm["min-length"].as<std::size_t>(), vm["skip_polyA"].as<bool>(), vm["skip_polyT"].as<bool>(), vm["min-trim"].as<std::size_t>(), vm["window-size"].as<std::size_t>(), vm["perfect-windows"].as<std::size_t>(), vm["max-trim"].as<std::size_t>(), vm["max-mismatch-errorDensity"].as<double>(), vm["stranded"].as<bool>(), vm["no-left"].as<bool>(), vm["no-right"].as<bool>(), vm["no-orphans"].as<bool>() );
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
