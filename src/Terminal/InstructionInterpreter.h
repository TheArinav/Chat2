#ifndef INSTRUCTION_INTERPRETER_H
#define INSTRUCTION_INTERPRETER_H

#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <any>

using std::string;
using std::vector;
using std::any;

namespace terminal {
    enum class Context {
        ANY=-1,
        NONE=0,
        SERVER=1,
        CLIENT_LOGGED_OUT=2,
        CLIENT_LOGGED_IN=3,
        CLIENT_LOGGED_IN_NO_ROOM=4,
        CLIENT_LOGGED_IN_ROOM=5
    };
    struct DataTypeInfo{
        string Descriptor;
        string Name;
        string ValidChars;
        string Opener;
        string Closer;
        DataTypeInfo(string des, string name, string chars, string open,string close):
        Descriptor(std::move(des)), Name(std::move(name)), ValidChars(std::move(chars)), Opener(std::move(open))
        ,Closer(std::move(close)){}
    };
    struct ParameterType{
        string ShortForm;
        string LongForm;
        string TypeDescriptor;
        int CategoryIndex;
    };
    struct InstructionType{
        string ShortForm;
        string LongForm;
        Context ValidContext;
        vector<ParameterType> Parameters;
        int requiredParamCount;
    };
    struct Parameter{
        ParameterType *Type;
        any Value;
    };
    struct Instruction{
        InstructionType *Type;
        vector<Parameter> Params;
    };
    namespace InstructionInterpreter{
        static vector<ParameterType> ParamTypes;
        static vector<InstructionType> InstTypes;
        static vector<DataTypeInfo> TypesInfo;
        void Setup();
        Instruction Parse(const string&);
        static InstructionType ProcessInstruction(const string &inp);
        static string IsLegalInst(const string&);
    }
}

#endif