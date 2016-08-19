#ifndef IOHANDLER_H
#define IOHANDLER_H

#include <istream>
#include <memory>
#include "read.h"
#include <boost/iostreams/concepts.hpp>
#include <boost/algorithm/string.hpp>

// ### input ###
template <class T, class Impl>
class InputReader : Impl {
public:
    typedef std::unique_ptr<T> value_type;
    using Impl::Impl;
    
    bool has_next();
    value_type next();
};

class InputFastq {
protected:
    Read load_read(std::istream *input);
    
    std::string id, seq, id2, qual;
};

class SingleEndReadFastqImpl : public InputFastq{
public:
    SingleEndReadFastqImpl(std::istream& in) : input(&in) {}
    
protected:
    std::istream* input = 0;
};

class PairedEndReadFastqImpl : public InputFastq {
public:
    PairedEndReadFastqImpl(std::istream& in1_, std::istream& in2_) : in1(&in1_), in2(&in2_) {}
  
protected:
    std::istream* in1, * in2 = 0;
};

class TabReadImpl : public InputFastq {
public:
    TabReadImpl(std::istream& in1_) : in1(&in1_) {}
    std::vector<Read> load_read(std::istream *input);
protected:
    std::istream* in1;
};

class InterReadImpl : public InputFastq {
public:
    InterReadImpl(std::istream& in1_) : in1(&in1_) {}
protected:
    std::istream *in1;
};

// ### output ###

template <class T, class Impl>
class OutputWriter : Impl {
public:
    using Impl::Impl;
    void write(const T& data);
};

class OutputFastq {
protected:
    void write_read(const Read& read, std::ostream &output);
};


class SingleEndReadOutFastq : public OutputFastq {
public:
    SingleEndReadOutFastq(std::ostream& out) : output(out) {}
    ~SingleEndReadOutFastq() { output.flush(); }
protected:
    std::ostream& output;
};

class PairedEndReadOutFastq : public OutputFastq {
public:
    PairedEndReadOutFastq(std::ostream& out1_, std::ostream& out2_) : out1(out1_), out2(out2_) {}
    ~PairedEndReadOutFastq() { out1.flush(); out2.flush(); }
protected:
    std::ostream &out1, &out2;
};

class PairedEndReadOutInter : public OutputFastq {
public:
    PairedEndReadOutInter(std::ostream& out1_) : out1(out1_) {}
    ~PairedEndReadOutInter() { out1.flush(); }
protected:
    std::ostream &out1;
};

class OutputTab {
protected:
    void write_read(const Read& read1, const Read& read2, std::ostream &output);
    void write_read(const Read& read, std::ostream &output);
};

class ReadBaseOutTab : public OutputTab {
public:
    ReadBaseOutTab(std::ostream& _output) : output(_output) {}
    ~ReadBaseOutTab() { output.flush(); }
protected:
    std::ostream &output;
};


#endif
