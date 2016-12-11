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

#include "overlapper.h"

#define AST_COUNT 4096
namespace
{
    const size_t SUCCESS = 0;
    const size_t ERROR_IN_COMMAND_LINE = 1;
    const size_t ERROR_UNHANDLED_EXCEPTION = 2;

} // namespace

typedef std::unordered_map <std::string, size_t> Counter;

namespace bi = boost::iostreams;

int main(int argc, char** argv)
{
    const std::string program_name = "Overlapper";

    Counter counters;
    setupCounter(counters); 

    std::string prefix;
    std::vector<std::string> default_outfiles = {"PE1", "PE2", "SE"};

    bool fastq_out;
    bool tab_out;
    bool std_out;
    bool std_in;
    bool gzip_out;
    bool interleaved_out;
    bool force; 

    bool checkR2;

    size_t maxMismatch;
    size_t minLength;
    size_t minOverlap;
    bool adapterTrimming;
    bool stranded;
    std::string histFile;

    std::string statsFile;
    bool appendStats;

    try
    {
        /** Define and parse the program options
         */
        namespace po = boost::program_options;
        po::options_description desc("Options");
        desc.add_options()
            ("version,v",                  "Version print")
            ("read1-input,1", po::value< std::vector<std::string> >(),
                                           "Read 1 input <comma sep for multiple files>") 
            ("read2-input,2", po::value< std::vector<std::string> >(), 
                                           "Read 2 input <comma sep for multiple files>")
            ("singleend-input,U", po::value< std::vector<std::string> >(),
                                           "Single end read input <comma sep for multiple files>")
            ("tab-input,T", po::value< std::vector<std::string> >(),
                                           "Tab input <comma sep for multiple files>")
            ("interleaved-input,I", po::value< std::vector<std::string> >(),
                                           "Interleaved input I <comma sep for multiple files>")
            ("stdin-input,S", po::bool_switch(&std_in)->default_value(false), "STDIN input <MUST BE TAB DELIMITED INPUT>")
            ("gzip-output,g", po::bool_switch(&gzip_out)->default_value(false),  "Output gzipped")
            ("interleaved-output,i", po::bool_switch(&interleaved_out)->default_value(false),     "Output to interleaved")
            ("fastq-output,f", po::bool_switch(&fastq_out)->default_value(false), "Fastq format output")
            ("force,F", po::bool_switch(&force)->default_value(false),         "Forces overwrite of files")
            ("tab-output,t", po::bool_switch(&tab_out)->default_value(false),   "Tab-delimited output")
            ("to-stdout,O", po::bool_switch(&std_out)->default_value(false),    "Prints to STDOUT in Tab Delimited")
            ("prefix,p", po::value<std::string>(&prefix)->default_value("overlapped_"),
                                           "Prefix for outputted files")
            ("minLength,l", po::value<size_t>(&minLength)->default_value(50), "Mismatches allowed in overlapped section")
            ("max-mismatches,x", po::value<size_t>(&maxMismatch)->default_value(5), "Mismatches allowed in overlapped section")
            ("min-overlap,o", po::value<size_t>(&minOverlap)->default_value(8), "Min overlap required to merge two reads")
            ("adapter-trimming,a", po::bool_switch(&adapterTrimming)->default_value(false), "Trims adapters based on overlap, only returns PE reads, will correct quality scores and BP in the PE reads")
            ("stranded,s", po::bool_switch(&stranded)->default_value(false), "Makes sure the correct complement is returned upon overlap")
            ("hist-file,l", po::value<std::string>(&histFile)->default_value(""), "A tab delimited hist file with insert lengths.")
            ("stats-file,L", po::value<std::string>(&statsFile)->default_value("stats.log") , "String for output stats file name")
            ("append-stats-file,A", po::bool_switch(&appendStats)->default_value(false),  "Append Stats file.")
            ("help,h",                     "Prints help.");

        po::variables_map vm;
        try
        {
            po::store(po::parse_command_line(argc, argv, desc),
                      vm); // can throw

            /** --help option
             */
            if ( vm.count("help")  || vm.size() == 0)
            {
                std::cout << "Tab-Converter" << std::endl
                          << desc << std::endl;
                return SUCCESS;
            }

            po::notify(vm); // throws on error, so do after help in case
            //Index 1 start location (making it more human friendly)
            
            std::shared_ptr<HtsOfstream> out_1 = nullptr;
            std::shared_ptr<HtsOfstream> out_2 = nullptr;
            std::shared_ptr<HtsOfstream> out_3 = nullptr;
            
            std::shared_ptr<OutputWriter> pe = nullptr;
            std::shared_ptr<OutputWriter> se = nullptr;
            
            if (fastq_out || (! std_out && ! tab_out) ) {
                for (auto& outfile: default_outfiles) {
                    outfile = prefix + outfile + ".fastq";
                }
                
                out_1.reset(new HtsOfstream(default_outfiles[0], force, gzip_out, false));
                out_2.reset(new HtsOfstream(default_outfiles[1], force, gzip_out, false));
                out_3.reset(new HtsOfstream(default_outfiles[2], force, gzip_out, false));

                pe.reset(new PairedEndReadOutFastq(out_1, out_2));
                se.reset(new SingleEndReadOutFastq(out_3));
            } else if (interleaved_out)  {
                for (auto& outfile: default_outfiles) {
                    outfile = prefix + "INTER" + ".fastq";
                }

                out_1.reset(new HtsOfstream(default_outfiles[0], force, gzip_out, false));
                out_3.reset(new HtsOfstream(default_outfiles[1], force, gzip_out, false));

                pe.reset(new PairedEndReadOutInter(out_1));
                se.reset(new SingleEndReadOutFastq(out_3));
            } else if (tab_out || std_out) {
                for (auto& outfile: default_outfiles) {
                    outfile = prefix + "tab" + ".tastq";
                }
                out_1.reset(new HtsOfstream(default_outfiles[0], force, gzip_out, std_out));

                pe.reset(new ReadBaseOutTab(out_1));
                se.reset(new ReadBaseOutTab(out_1));
            }
            histVec insertLengths;

            if (histFile == "") {
                insertLengths = nullptr;
            } else {
                insertLengths = histVec(new std::vector<unsigned long long int>);
            }
            
            //setLookup(lookup, lookup_rc, readPhix);
            // there are any problems
            if(vm.count("read1-input")) {
                if (!vm.count("read2-input")) {
                    throw std::runtime_error("must specify both read1 and read2 input files.");
                } else if (vm.count("read2-input") != vm.count("read1-input")) {
                    throw std::runtime_error("must have same number of input files for read1 and read2");
                }
                auto read1_files = vm["read1-input"].as<std::vector<std::string> >();
                auto read2_files = vm["read2-input"].as<std::vector<std::string> >();

                for(size_t i = 0; i < read1_files.size(); ++i) {
                    bi::stream<bi::file_descriptor_source> is1{check_open_r(read1_files[i]), bi::close_handle};
                    bi::stream<bi::file_descriptor_source> is2{check_open_r(read2_files[i]), bi::close_handle};
                    InputReader<PairedEndRead, PairedEndReadFastqImpl> ifp(is1, is2);
                    helper_overlapper(ifp, pe, se, counters, maxMismatch,  minOverlap, insertLengths, stranded, minLength,  adapterTrimming);
                }
            }

            if(vm.count("singleend-input")) {
                auto read_files = vm["singleend-input"].as<std::vector<std::string> >();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> sef{ check_open_r(file), bi::close_handle};
                    InputReader<SingleEndRead, SingleEndReadFastqImpl> ifs(sef);
                    //JUST WRITE se read out - no way to overlap
                    helper_overlapper(ifs, pe, se, counters, maxMismatch,  minOverlap, insertLengths, stranded, minLength,  adapterTrimming);
                }
            }
            
            if(vm.count("tab-input")) {
                auto read_files = vm["tab-input"].as<std::vector<std::string> > ();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> tabin{ check_open_r(file), bi::close_handle};
                    InputReader<ReadBase, TabReadImpl> ift(tabin);
                    helper_overlapper(ift, pe, se, counters, maxMismatch,  minOverlap, insertLengths, stranded, minLength,  adapterTrimming);
                }
            }
            
            if (vm.count("interleaved-input")) {
                auto read_files = vm["interleaved-input"].as<std::vector<std::string > >();
                for (auto file : read_files) {
                    bi::stream<bi::file_descriptor_source> inter{ check_open_r(file), bi::close_handle};
                    InputReader<PairedEndRead, InterReadImpl> ifp(inter);
                    helper_overlapper(ifp, pe, se, counters, maxMismatch,  minOverlap, insertLengths, stranded, minLength,  adapterTrimming);
                }
            }
           
            if (std_in) {
                bi::stream<bi::file_descriptor_source> tabin {fileno(stdin), bi::close_handle};
                InputReader<ReadBase, TabReadImpl> ift(tabin);
                helper_overlapper(ift, pe, se, counters, maxMismatch,  minOverlap, insertLengths, stranded, minLength,  adapterTrimming);
            }  


            if (insertLengths) {
                std::ofstream histOutputFile(histFile);
                //0 is reserved for no overlap
                std::string stars;
                for (int i = 1; i < insertLengths->size(); ++i) {
                    stars = "";
                    if ((*insertLengths)[i]) {
                        stars = stars.insert(0, (*insertLengths)[i]/AST_COUNT, '*');
                        histOutputFile << i << '\t' << (*insertLengths)[i] << '\t' << stars << '\n';
                    }
                }
                stars = stars.insert(0, (*insertLengths)[0]/AST_COUNT, '*');
                histOutputFile << "None" << '\t' << (*insertLengths)[0] << '\t' << stars << '\n';
                histOutputFile.close();
            }
            write_stats(statsFile, appendStats, counters, program_name);
        }
        catch(po::error& e)
        {
            std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
            std::cerr << desc << std::endl;
            return ERROR_IN_COMMAND_LINE;
        }

    }
    catch(std::exception& e)
    {
        std::cerr << "\n\tUnhandled Exception: "
                  << e.what() << std::endl;
        return ERROR_UNHANDLED_EXCEPTION;

    }

    std::cerr << "TotalRecords:" << counters["TotalRecords"] << '\n';
    return SUCCESS;

}

