//
// Created by dc on 24/12/18.
//

#include <suil/console.h>
#include <suil/file.h>
#include <suil/utils.h>
#include "program.h"

namespace suil::scc {

#define spaces(n) suil::String(' ', n)

    static void generateprogramFileSymbols(ProgramFile &pf, File &out) {
        std::map<std::string, bool> added;
        auto addSymbol = [&](const std::string &name) {
            if (added.find(name) != added.end())
                return;

            out << "#ifndef IOD_SYMBOL_" << name << "\n"
                << "#define IOD_SYMBOL_" << name << "\n"
                << "    iod_define_symbol(" << name << ")\n"
                << "#endif\n\n";
            added[name] = true;
        };

        for (auto &ft: pf.Symbols) {
            // add floating definitions first
            addSymbol(ft);
        }

        for (auto &tp: pf.MetaTypes) {
            // add field symbols defined for each type
            if (tp.Kind == "union") {
                addSymbol("Type");
                addSymbol("Data");
                continue;
            }

            for (auto field: tp.Fields) {
                addSymbol(field.Name);
                for (auto attr: field.Attribs) {
                    if (attr.isSimple())
                        addSymbol(attr.Resolved);
                }
            }
        }
    }

    static void generateMetaUnion(const MetaType& tp, File& out)
    {
        // union only methods
        out << spaces(8) << "enum Types: std::uint32_t {\n";
        int index{1};
        for (auto& field: tp.Fields) {
            out << spaces(12) << field.Name;
            if (index != tp.Fields.size()) {
                out << ",\n";
            }
            else {
                out << "\n";
            }
        }
        out << spaces(8) << "};\n\n";

        out << spaces(8) << "template <typename S>\n"
            << spaces(8) << "static " << tp.Name << " create(const S& src) {\n"
            << spaces(12) << "auto it = mUnionEntries.find(std::type_index(typeid(T)));\n"
            << spaces(12) << "if (it == mUnionEntries.end()) {\n"
            << spaces(16) << R"(throw Exception::create("Cannot convert '", typeid(T).name(), "' to )" << tp.Name << "'\");\n"
            << spaces(12) << "}\n"
            << spaces(12) << tp.Name << " u;\n"
            << spaces(12) << "u.mType = it->second;\n"
            << spaces(12) << "suil::Heapboard hb(src.maxBytesSize);\n"
            << spaces(12) << "hb << src;\n"
            << spaces(12) << "u.mData = hb.release();\n"
            << spaces(12) << "return u;\n"
            << spaces(8) << "}\n\n";

        out << spaces(8) << "template <typename D>\n"
            << spaces(8) << "suil::Status get(D& dst) {\n"
            << spaces(12) << "auto it = mUnionEntries.find(std::type_index(typeid(T)));\n"
            << spaces(12) << "if (it == mUnionEntries.end()) {\n"
            << spaces(16) << "return suil::Failure{\"Cannot convert " << tp.Name << "to type '\", typeid(T).name(), \"', type not supported\"};\n"
            << spaces(12) << "}\n"
            << spaces(12) << "if (it->second != Ego.Type) {\n"
            << spaces(16) << "return suil::Failure{\"Cannot convert " << tp.Name << " to type '\", typeid(T).name(), \"', data is of type \", Ego.Type};\n"
            << spaces(12) << "}\n"
            << spaces(12) << "suil::Heapboard hb(Ego.Data);\n"
            << spaces(12) <<  "try {\n"
            << spaces(16) <<  "hb >> dst;\n"
            << spaces(16) <<  "return suil::Success;\n"
            << spaces(12) <<  "}\n"
            << spaces(12) <<  "catch (...) {\n"
            << spaces(16) <<  "return suil::Failure{Exception::fromCurrent().what()};\n"
            << spaces(12) <<  "}\n"
            << spaces(8) <<  "}\n\n";

        out << spaces(8) << "template <typename D>\n"
            << spaces(8) << "D get() {\n"
            << spaces(12) << "D dst;\n"
            << spaces(12) << "auto ok = Ego.get(dst);\n"
            << spaces(12) << "if (!ok) {\n"
            << spaces(16) << "throw Exception::create(ok.Error);\n"
            << spaces(12) << "}\n"
            << spaces(12) << "return dst;\n"
            << spaces(8) << "}\n\n";
    }

    static void generateMetaTypeHeaders(ProgramFile &pf, File &out) {
        for (auto &tp: pf.MetaTypes) {
            // add type
            out << spaces(4) << "struct " << tp.Name << ": iod::MetaType";
            if (!tp.Base.empty())
                out << ", public " << tp.Base;
            out << " {\n\n";

            // type schema
            out << spaces(8) << "typedef decltype(iod::D(\n";
            bool isFirst{true};
            static std::vector<Field> unionFields = {
                    Field{{}, "std::uint32_t", "Type"},
                    Field{{}, "suil::Data", "Data"}
            };
            auto& fields = tp.Kind == "meta"? tp.Fields : unionFields;

            for (auto &field: fields) {
                if (!isFirst) out << ",\n";
                out << spaces(12) << "prop(" << field.Name;
                if (!field.Attribs.empty()) {
                    // append attributes
                    Map<bool> included;
                    bool attrFirst{true};
                    out << "(";
                    for (auto &attr : field.Attribs) {
                        // add all attributes
                        if (!attrFirst) out << ", ";
                        auto attrStr = String{(attr.isSimple() ? attr.Resolved : attr.Parts.back())};
                        if (included.find(attrStr) == included.end()) {
                            // only include attributes if not already included
                            out << "var(" << attrStr << ")";
                            attrFirst = false;
                            included.emplace(attrStr.peek(), true);
                        }
                    }
                    out << ")";
                }
                out << ", " << field.FieldType << ")";
                isFirst = false;
            }
            out << "\n"
                << spaces(8) << ")) Schema" << ";\n\n";

            // add serialization methods
            out << spaces(8) << "static " << tp.Name << " fromJson(iod::json::parser&);\n\n"
                << spaces(8) << "void toJson(iod::json::jstream&) const;\n\n"
                << spaces(8) << "size_t maxByteSize() const;\n\n"
                << spaces(8) << "static " << tp.Name << " fromWire(suil::Wire&);\n\n"
                << spaces(8) << "void toWire(suil::Wire&) const;\n\n"
                << spaces(8) << "friend suil::OBuffer& operator<<(suil::OBuffer& out, const " << tp.Name << "& o);\n\n"
                << spaces(8) << "static Schema Meta;\n\n";

            if (tp.Kind == "union") {
                out << "/* Union methods */\n\n";
                generateMetaUnion(tp, out);
                out << spaces(4) << "private:\n";
                // declare union types map
                out << spaces(8) << "static std::unordered_map<std::type_index, std::uint32_t> mUnionEntries;\n\n";
            }
            // add type fields
            for (auto &field: fields) {
                out << spaces(8) << field.FieldType << " " << field.Name << ";\n\n";
            }

            out << spaces(4) << "};\n\n";
        }
    }

    static void generateServiceHeaders(ProgramFile &pf, File &out) {
        auto appendMethods = [&](const std::vector<Method> &methods) {
            for (auto &m: methods) {
                out << "        " << m.ReturnType << " " << m.Name << "(";
                bool first = true;
                for (auto &p : m.Params) {
                    if (!first)
                        out << ", ";
                    first = false;
                    if (p.IsConst)
                        out << "const ";
                    out << p.ParameterType;
                    if (p.Kind == Parameter::Move)
                        out << "&&";
                    else if (p.Kind == Parameter::Reference)
                        out << "&";
                    out << " " << p.Name;
                }
                out << ");\n\n";
            }
        };

        auto appendCtors = [&](const std::vector<Constructor> &ctors) {
            for (auto &m: ctors) {
                out << spaces(8) << m.Name << "(";
                bool first = true;
                for (auto &p : m.Params) {
                    if (!first)
                        out << ", ";
                    first = false;
                    if (p.IsConst)
                        out << "const ";
                    out << p.ParameterType;
                    if (p.Kind == Parameter::Move)
                        out << "&&";
                    else if (p.Kind == Parameter::Reference)
                        out << "&";
                    out << " " << p.Name;
                }
                out << ");\n\n";
            }
        };

        auto generateForJrpc = [&](const RpcType &svc) {
            // start with with client
            out << spaces(4) << "struct j" << svc.Name << "Client: suil::rpc::JsonRpcClient {\n\n"
                << spaces(4) << "public:\n\n";
            appendMethods(svc.Methods);
            out << spaces(4) << "};\n\n";
            // add service server handler
            out << spaces(4) << svc.Kind << " j" << svc.Name << "Handler : " << svc.Name
                << ", suil::rpc::JsonRpcHandler {\n\n";
            out << spaces(4) << "protected:\n\n"
                << spaces(8) << "suil::rpc::ReturnType operator()("
                                "const suil::String& method, const suil::json::Object& params, int id) override;\n"
                << spaces(4) << "};\n\n";
        };

        auto generateForSuil = [&](const RpcType &svc) {
            // start with with client
            out << spaces(4) << "struct s" << svc.Name << "Client: suil::rpc::SuilRpcClient {\n\n"
                << spaces(4) << "public:\n\n";
            appendMethods(svc.Methods);
            out << spaces(4) << "};\n\n";
            // add service server handler
            out << spaces(4) << svc.Kind << " s" << svc.Name << "Handler : " << svc.Name
                << ", suil::rpc::SuilRpcHandler {\n\n"
                << spaces(8) << "s" << svc.Name << "Handler();\n\n";
            out << spaces(4) << "protected:\n\n"
                << spaces(8) << "suil::Result operator()("
                                "suil::Breadboard& results, int method, suil::Breadboard& params, int id) override;\n"
                << spaces(4) << "};\n\n";
        };

        for (auto &svc: pf.Services) {
            out << spaces(4) << svc.Kind << " " << svc.Name;
            if (!svc.Base.empty())
                  out << ": public " << svc.Base;
            out << " {\n\n";

            appendCtors(svc.Ctors);
            appendMethods(svc.Methods);
            out << spaces(4) << "};\n\n";
            auto both = (svc.Kind == "srvc");
            // start with with client
            if (both || (svc.Kind == "srpc"))
                generateForSuil(svc);
            if (both || (svc.Kind == "jrpc"))
                generateForJrpc(svc);
        }
    }

    static void generateJsonRpcSources(File &sf, scc::RpcType &svc) {
        // start by implementing handler
        sf << spaces(4) << "ReturnType j" << svc.Name
           << "Handler::operator()(const suil::String& method, const suil::json::Object& params, int id)\n"
           << spaces(4) << "{\n";
        sf << spaces(8) << "static const suil::Map<int> scMappings = {\n";
        int id = 0;
        for (auto &m: svc.Methods) {
            // append method mappings
            if (id != 0)
                sf << ",\n";
            sf << spaces(12) << "{\"" << m.Name << "\"," << utils::tostr(id++) << "}";
        }
        sf << "};\n\n";
        sf << spaces(8) << "auto it = scMappings.find(method);\n"
           << spaces(8) << "if (it == scMappings.end())\n"
           << spaces(8) << "{\n"
           << spaces(12) << "// method not found\n"
           << spaces(12) << "return std::make_pair(JRPC_METHOD_NOT_FOUND,"
           << " suil::json::Object(\"method does not exists\"));\n"
           << spaces(8) << "}\n"
           << "\n"
           << spaces(8) << "switch(it->second) {\n";
        id = 0;
        for (auto &m: svc.Methods) {
            // append method handling cases
            sf << spaces(12) << "case " << utils::tostr(id++) << ": {\n";
            OBuffer ob(32);
            bool first{true};
            for (auto &p: m.Params) {
                sf << spaces(16) << "" << p.ParameterType << " " << p.Name << " = (" << p.ParameterType << ") "
                   << "params[\"" << p.Name << "\"];\n";
                if (!first)
                    ob << ", ";
                first = false;
                if (p.Kind == Parameter::Move)
                    ob << "std::move(";
                ob << p.Name;
                if (p.Kind == Parameter::Move)
                    ob << ")";
            }
            sf << "\n";
            if (m.ReturnType != "void") {
                // void methods will return nullptr
                sf << spaces(16) << "return std::make_pair(0, suil::json::Object(Ego."
                   << m.Name << "(" << ob << ")));\n";
            } else {
                sf << spaces(16) << "Ego." << m.Name << "(" << ob << ");\n";
                sf << spaces(16) << "return std::make_pair(0, suil::json::Object(nullptr));\n";
            }
            sf << spaces(12) << "}\n";
        }
        sf << spaces(12) << "default:\n"
           << spaces(16) << "  // method not found\n"
           << spaces(16) << "  return std::make_pair(JRPC_METHOD_NOT_FOUND,"
           << " suil::json::Object(\"method does not exists\"));\n"
           << spaces(12) << "}\n"
           << spaces(8) << "}\n\n";

        // implement client
        for (auto &m: svc.Methods) {
            sf << spaces(4) << m.ReturnType << " j" << svc.Name << "Client::" << m.Name << "(";
            bool first = true;
            OBuffer ob(32);
            for (auto &p : m.Params) {
                if (!first) {
                    sf << ", ";
                    ob << ", ";
                }
                first = false;
                if (p.IsConst)
                    sf << "const ";
                sf << p.ParameterType;
                if (p.Kind == Parameter::Move)
                    sf << "&&";
                else if (p.Kind == Parameter::Reference)
                    sf << "&";
                sf << " " << p.Name;
                ob << "\"" << p.Name << "\", " << p.Name;
            }
            sf << ")\n"
               << spaces(4) << "{\n";
            if (m.Params.empty())
                sf << spaces(8) << "suil::json::Object params(nullptr);";
            else
                sf << spaces(8) << "suil::json::Object params(suil::json::Obj, " << ob << ");";
            sf << "\n"
               << spaces(8) << "auto ret = Ego.call(\"" << m.Name << "\", std::move(params));\n"
               << spaces(8) << "if (ret.first)\n"
               << spaces(12) << "// api error\n"
               << spaces(12) << "throw suil::Exception::create((suil::String)ret.second);\n\n";
            if (m.ReturnType != "void")
                sf << spaces(8) << "return (" << m.ReturnType << ") ret.second;\n";
            sf << spaces(4) << "}\n\n";
        }
    }

    static void generateSuilRpcSources(File &sf, scc::RpcType &svc) {
        sf << spaces(4) << "s" << svc.Name << "Handler::s" << svc.Name << "Handler() : "
           << svc.Name << "(), suil::rpc::SuilRpcHandler()\n"
           << spaces(4) << "{\n";
        int id = 1;
        for (auto &m: svc.Methods) {
            sf << spaces(8) << "Ego.appendMethod(" << utils::tostr(id++) << ", \"" << m.Name << "\");\n";
        }
        sf << spaces(4) << "}\n\n";

        sf << spaces(4) << "suil::Result s" << svc.Name
           << "Handler::operator()(suil::Breadboard& results, int method, suil::Breadboard& params, int id)\n"
           << spaces(4) << "{\n"
           << spaces(8) << "switch(method) {\n";

        id = 1;
        for (auto &m: svc.Methods) {
            // append method handling cases
            sf << spaces(12) << "case " << utils::tostr(id++) << ": {\n";
            OBuffer ob(32);
            bool first{true};
            for (auto &p: m.Params) {
                sf << spaces(16) << "" << p.ParameterType << " " << p.Name << "{};\n"
                   << spaces(16) << "params >> " << p.Name << ";\n";
                if (!first)
                    ob << ", ";
                first = false;
                if (p.Kind == Parameter::Move)
                    ob << "std::move(";
                ob << p.Name;
                if (p.Kind == Parameter::Move)
                    ob << ")";
            }
            sf << "\n";
            if (m.ReturnType == "void") {
                // void methods will return nullptr
                sf << spaces(16) << "Ego." << m.Name << "(" << ob << ");\n";
            } else {
                sf << spaces(16) << m.ReturnType << " tmp = Ego." << m.Name << "(" << ob << ");\n";
                sf << spaces(16) << "results << tmp;\n";
            }
            sf << spaces(16) << "return suil::Result(0);\n"
               << spaces(12) << "}\n";
        }
        sf << spaces(12) << "default:\n"
           << spaces(16) << "// method not found\n"
           << spaces(16) << "suil::Result res(SRPC_METHOD_NOT_FOUND);\n"
           << spaces(16) << "res << \"requested method does not exist\";\n"
           << spaces(16) << "return std::move(res);\n"
           << spaces(12) << "}\n"
           << spaces(8) << "}\n\n";

        // implement client
        for (auto &m: svc.Methods) {
            sf << spaces(4) << m.ReturnType << " s" << svc.Name << "Client::" << m.Name << "(";
            bool first = true;
            OBuffer ob(32);
            for (auto &p : m.Params) {
                if (!first) {
                    sf << ", ";
                    ob << ", ";
                }
                first = false;
                if (p.IsConst)
                    sf << "const ";
                sf << p.ParameterType;
                if (p.Kind == Parameter::Move)
                    sf << "&&";
                else if (p.Kind == Parameter::Reference)
                    sf << "&";
                sf << " " << p.Name;
                ob << p.Name;
            }
            auto ret = (m.ReturnType == "void") ? "" : "return ";
            sf << ")\n"
               << spaces(4) << "{\n";
            if (m.Params.empty())
                sf << spaces(8) << ret << "Ego.call<" << m.ReturnType << ">(\"" << m.Name << "\");\n";
            else
                sf << spaces(8) << ret << "Ego.call<" << m.ReturnType << ">(\"" << m.Name << "\", " << ob << ");\n";
            sf << spaces(4) << "}\n\n";
        }
    }

    void ProgramFile::generateHeaderFile(const suil::String &path) {
        if (utils::fs::exists(path())) {
            // remove current file
            utils::fs::remove(path());
        }

        File hf(path(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        // append guard
        hf << "#pragma once\n\n";
        // append includes
        hf << "#include <suil/result.h>\n"
           << "#include <suil/json.h>\n"
           << "#include <suil/wire.h>\n\n"
           << "#include <unordered_map>\n"
           << "#include <typeindex>\n";

        for (auto &inc: Ego.Includes) {
            // add all includes
            hf << inc << "\n";
        }
        hf << "\n";
        // append symbols
        generateprogramFileSymbols(Ego, hf);

        hf << "namespace " << Ego.Namespace << " {\n\n";

        generateMetaTypeHeaders(Ego, hf);
        generateServiceHeaders(Ego, hf);

        hf << "}\n";
        hf.flush();
        hf.close();
    }

    static void generateMetaTypeSources(File &sf, const MetaType &mt)
    {
        if (mt.Kind == "union") {
            // generation union entries map
            int index{0};
            sf << spaces(4) << "std::unordered_map<std::type_index, std::uint32_t> " << mt.Name << "::UnionEntries = {\n";
            for (auto& field: mt.Fields) {
                sf << spaces(8) << "{std::typeindex(typeid(" << field.FieldType << ")), " << std::to_string(index++) << "}";
                if (index != mt.Fields.size()) {
                    sf << ",\n";
                }
                else {
                    sf << "\n";
                }
            }
            sf << spaces(4) << "};\n\n";
        }
        /*
         * */
        sf << spaces(4) << mt.Name << "::Schema " << mt.Name << "::Meta{};\n\n";

        sf << spaces(4) << mt.Name << " " << mt.Name << "::fromJson(iod::json::parser& p)\n"
           << spaces(4) << "{\n"
           << spaces(8) << mt.Name << " tmp{};\n"
           << spaces(8) << "p >> p.spaces >> '{';\n"
           << spaces(8) << "iod::json::iod_attr_from_json(&" << mt.Name << "::Meta, tmp, p);\n"
           << spaces(8) << "p >> p.spaces >> '}';\n"
           << spaces(8) << "return tmp;\n"
           << spaces(4) << "}\n\n";

        sf << spaces(4) << "void " << mt.Name << "::toJson(iod::json::jstream& ss) const\n"
           << spaces(4) << "{\n"
           << spaces(8) << "suil::json::metaToJson(Ego, ss);\n"
           << spaces(4) << "}\n\n";

        sf << spaces(4) << "size_t " << mt.Name << "::maxByteSize() const\n"
           << spaces(4) << "{\n"
           << spaces(8) << "return suil::metaMaxByteSize(Ego);\n"
           << spaces(4) << "}\n\n";

        sf << spaces(4) << mt.Name << " " << mt.Name << "::fromWire(suil::Wire& w)\n"
           << spaces(4) << "{\n"
           << spaces(8) << mt.Name << " tmp{};\n"
           << spaces(8) << "suil::metaFromWire(tmp, w);\n"
           << spaces(8) << "return std::move(tmp);\n"
           << spaces(4) << "}\n\n";

        sf << spaces(4) << "void " << mt.Name << "::toWire(suil::Wire& w) const\n"
           << spaces(4) << "{\n"
           << spaces(8) << "suil::metaToWire(Ego, w);\n"
           << spaces(4) << "}\n\n";

        sf << spaces(4) << "suil::OBuffer& operator<<(suil::OBuffer& o, const " << mt.Name << "& a)\n"
           << spaces(4) << "{\n"
           << spaces(8) << "o << suil::json::encode(a);\n"
           << spaces(4) << "}\n\n";
    }

    void ProgramFile::generateSourceFile(const char *filename, const String &spath)
    {
        if (utils::fs::exists(spath())) {
            // remove current file
            utils::fs::remove(spath());
        }

        File sf(spath(), O_WRONLY|O_CREAT|O_APPEND, 0666);
        sf << "/* file generated by suil's scc program, do not modify */\n\n"
           << "#include \"" << filename << ".h\"\n\n";
        if (!Ego.Services.empty()) {
            sf << "using namespace suil::rpc;\n\n";
        }

        sf << "namespace " << Ego.Namespace << " {\n\n";

        for (auto& tp: Ego.MetaTypes) {
            // generate serialization sources for the types
            generateMetaTypeSources(sf, tp);
        }

        // implement client methods
        for (auto& svc: Ego.Services) {

            auto both = (svc.Kind == "srvc");
            if (both || (svc.Kind == "jrpc")) {
                // generate json rpc
                generateJsonRpcSources(sf, svc);
            }

            if (both || (svc.Kind == "srpc")) {
                // generate suil rpc
                generateSuilRpcSources(sf, svc);
            }
        }

        sf << "}\n";

        sf.flush();
        sf.close();
    }

    void ProgramFile::generate(const char *filename, const char *outDir)
    {
        auto fname = utils::fs::getname(filename);

        String hdrFile, cppFile;
        if (outDir != nullptr) {
            if (!utils::fs::exists(outDir)) {
                // output directory does not exist, attempt to create
                utils::fs::mkdir(outDir, true);
            }

            hdrFile = utils::catstr(outDir, "/", fname, ".h");
            cppFile = utils::catstr(outDir, "/", fname, ".cpp");
        }
        else {
            hdrFile = utils::catstr(filename, ".h");
            cppFile = utils::catstr(filename, ".cpp");
        }

        // generate header file
        generateHeaderFile(hdrFile);
        generateSourceFile(fname(), cppFile);
    }
}
