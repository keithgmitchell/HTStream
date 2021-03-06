#include "gtest/gtest.h"
#include <sstream>
#include <iostream>
#include "hts_SeqScreener.h"

class SeqScreenerTest : public ::testing::Test {
public:
    const std::string phixTest = "ACTGACTGACTGACTGACTGACTGACTG";
    const std::string readData_1 = "@R1\nAAAAACTGACTGACTGTTTT\n+\nAAAAACTGACTGACTGTTTT\n";
    const size_t lookup_kmer_test = 2;
    SeqScreener ss;
};

TEST_F(SeqScreenerTest, check_fasta) {
    const std::string faFile = ">1\nAGCTAGCT\n>2\nCCGGAATTCC\n";
    std::istringstream fa(faFile);
    InputReader<SingleEndRead, FastaReadImpl> f(fa);

    kmerSet lookup;
    uint64_t screen_len;
    size_t true_kmer = 5;
    screen_len = ss.setLookup_fasta(lookup, f, true_kmer);

    std::cout << "Length should be 18 == " << screen_len << '\n';
    ASSERT_EQ(18u, screen_len);
    std::cout << "Lookup size should be 6 == " << lookup.size() << '\n';
    ASSERT_EQ(6u,lookup.size());
}

TEST_F(SeqScreenerTest, check_check_read) {
    std::string s("AAAAAAAGCT");
    Read readPhix = Read(s, "", "");
    Read testRead = Read(s, "", "");
    kmerSet lookup;
    size_t true_kmer = 5;
    ss.setLookup_read(lookup, readPhix, true_kmer);

    boost::dynamic_bitset<> fLu(true_kmer * 2);
    boost::dynamic_bitset<> rLu(true_kmer * 2);
    size_t lookup_loc = 0;
    size_t lookup_loc_rc = (true_kmer * 2) -2;
    double val = ss.check_read(lookup, testRead, true_kmer * 2, lookup_loc, lookup_loc_rc, fLu, rLu );
    std::cout << "Hits should equal 6 == " << val << '\n';
    ASSERT_EQ(6u, val);
};;


TEST_F(SeqScreenerTest, setLookupTestOrderedVec) {
    std::string s("AAAAAAAGCT");
    std::cout << "Testing Building Lookup Table with sequence " << s << '\n';
    Read readPhix = Read(s, "", "");
    kmerSet lookup;

    ss.setLookup_read(lookup, readPhix, 5);
    std::cout << lookup.size() << '\n';
    ASSERT_EQ(4u, lookup.size());
};

TEST_F(SeqScreenerTest, setLookupTest) {
    std::string s("AAAAAAAGCT");
    std::cout << "Testing Building Lookup Table with sequence " << s << '\n';
    Read readPhix = Read(s, "", "");
    kmerSet lookup;
    ss.setLookup_read(lookup, readPhix, 5);
    //ASSERT_EQ(true , lookup.end != lookup.find(boost::dynamic_bitset<>(10, "1001111111")))
    //std::string s("1001111111");

};

TEST_F(SeqScreenerTest, setLookupTestAmbiguities) {
    std::string s("GAAAAAAGCVM");
    std::cout << "Testing Building Lookup Table with sequence ambiguities " << s << '\n';
    Read readAmbiguities = Read(s, "", "");
    std::cout << "Seq in read " << readAmbiguities.get_seq() << '\n';
    kmerSet lookup;
    ss.setLookup_read(lookup, readAmbiguities, 5);
    std::cout << "myset contains:";
    for ( auto it = lookup.begin(); it != lookup.end(); ++it )
        std::cout << " " << *it;
    std::cout << std::endl;
    std::cout << lookup.size() << '\n';
    ASSERT_EQ(4u, lookup.size());
};

TEST_F(SeqScreenerTest, setLookupTestAmbiguities2_nokmers) {
    std::string s("GAANAAGCVM");
    std::cout << "Testing Building Lookup Table with sequence ambiguities " << s << '\n';
    Read readAmbiguities = Read(s, "", "");
    std::cout << "Seq in read " << readAmbiguities.get_seq() << '\n';
    kmerSet lookup;
    ss.setLookup_read(lookup, readAmbiguities, 5);
    std::cout << lookup.size() << '\n';
    ASSERT_EQ(0u, lookup.size());
};
