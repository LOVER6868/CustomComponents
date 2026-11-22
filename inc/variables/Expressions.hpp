#pragma once
#include "variables/Values.hpp"
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
namespace CustomComponents::Variables::Expressions {
struct Node {std::string token;int number=0;bool literal=false;std::vector<int> args;bool call=false;};
struct Program {std::vector<Node> nodes;int root=-1;};
class Parser {
    std::string_view s;std::size_t at=0;Program code;bool valid=true;int depth=0;
    void Space(){while(at<s.size() && (s[at]==' ' || s[at]=='\t'))++at;}
    bool Take(char c){Space();if(at<s.size() && s[at]==c){++at;return true;}return false;}
    int Add(Node node){if(code.nodes.size()>=128){valid=false;return -1;}code.nodes.push_back(std::move(node));return int(code.nodes.size()-1);}
    int Atom(){
        if(++depth>32){valid=false;return -1;}int result=-1;Space();
        if(Take('-'))result=Add({"neg",0,false,{Atom()}});
        else if(Take('+'))result=Atom();
        else if(Take('(')){result=Expr(0);if(!Take(')'))valid=false;}
        else if(at<s.size() && s[at]>='0' && s[at]<='9'){
            auto begin=at;while(at<s.size() && s[at]>='0' && s[at]<='9')++at;int n=0;auto read=std::from_chars(s.data()+begin,s.data()+at,n);if(read.ec!=std::errc{})valid=false;result=Add({"",n,true,{}});
        }else{
            auto begin=at;while(at<s.size() && ((s[at]>='a' && s[at]<='z') || (s[at]>='A' && s[at]<='Z') || (s[at]>='0' && s[at]<='9') || s[at]=='_'))++at;
            std::string name(s.substr(begin,at-begin));if(!Variables::Name(name)){valid=false;--depth;return -1;}
            Node node{name};if(Take('(')){node.call=true;if(!Take(')')){do{node.args.push_back(Expr(0));}while(Take(','));if(!Take(')'))valid=false;}}
            result=Add(std::move(node));
        }
        --depth;return result;
    }
    int Expr(int minimum){int left=Atom();for(;;){Space();if(at==s.size())break;char op=s[at];int precedence=(op=='+' || op=='-')?1:(op=='*' || op=='/' || op=='%')?2:0;if(!precedence || precedence<minimum)break;++at;int right=Expr(precedence+1);left=Add({std::string(1,op),0,false,{left,right}});}return left;}
public:
    explicit Parser(std::string_view text):s(text){}
    std::shared_ptr<const Program> Run(){if(s.empty() || s.size()>512)return {};code.root=Expr(0);Space();if(!valid || at!=s.size() || code.root<0)return {};return std::make_shared<const Program>(std::move(code));}
};
inline std::shared_ptr<const Program> Compile(std::string_view text){
    static std::mutex lock;static std::map<std::string,std::shared_ptr<const Program>,std::less<>> cache;
    {std::lock_guard<std::mutex> guard(lock);if(auto it=cache.find(text);it!=cache.end())return it->second;}
    auto result=Parser(text).Run();if(result){std::lock_guard<std::mutex> guard(lock);if(cache.size()>=256)cache.erase(cache.begin());cache[std::string(text)]=result;}return result;
}
using Lookup=std::function<std::optional<Value>(std::string_view)>;
inline std::optional<int> Evaluate(const Program& p,const Lookup& lookup,int node=-1,int depth=0){
    if(node<0)node=p.root;if(depth>32 || node<0 || node>=int(p.nodes.size()))return {};const auto& n=p.nodes[node];if(n.literal)return n.number;
    if(n.args.empty()){if(n.call)return {};auto v=lookup(n.token);return v?v->Integer():std::nullopt;}
    if((n.token=="len" || n.token=="sum") && n.args.size()==1){auto& arg=p.nodes[n.args[0]];if(arg.literal || arg.call || !arg.args.empty())return {};auto v=lookup(arg.token);if(!v || !v->Array())return {};if(n.token=="len")return int(v->items.size());if(v->kind!=Kind::Ints)return {};std::int64_t sum=0;for(int x:v->items)sum+=x;return sum>=INT32_MIN && sum<=INT32_MAX?std::optional<int>(int(sum)):std::nullopt;}
    if(n.token=="at" && n.args.size()==2){const auto& arg=p.nodes[n.args[0]];if(arg.literal || arg.call || !arg.args.empty())return {};auto v=lookup(arg.token);auto index=Evaluate(p,lookup,n.args[1],depth+1);if(!v || v->kind!=Kind::Ints || !index || *index<0 || *index>=int(v->items.size()))return {};return v->items[*index];}
    std::vector<int> a;for(int arg:n.args){auto value=Evaluate(p,lookup,arg,depth+1);if(!value)return {};a.push_back(*value);}std::int64_t r=0;
    if(n.token=="neg" && a.size()==1)r=-std::int64_t(a[0]);
    else if(n.token=="abs" && a.size()==1)r=a[0]<0?-std::int64_t(a[0]):a[0];
    else if(n.token=="min" && a.size()==2)r=std::min(a[0],a[1]);else if(n.token=="max" && a.size()==2)r=std::max(a[0],a[1]);
    else if(n.token=="clamp" && a.size()==3 && a[1]<=a[2])r=std::clamp(a[0],a[1],a[2]);
    else if(a.size()==2 && n.token=="+")r=std::int64_t(a[0])+a[1];else if(a.size()==2 && n.token=="-")r=std::int64_t(a[0])-a[1];else if(a.size()==2 && n.token=="*")r=std::int64_t(a[0])*a[1];
    else if(a.size()==2 && n.token=="/" && a[1])r=std::int64_t(a[0])/a[1];else if(a.size()==2 && n.token=="%" && a[1])r=std::int64_t(a[0])%a[1];else return {};
    return r>=INT32_MIN && r<=INT32_MAX?std::optional<int>(int(r)):std::nullopt;
}
inline std::optional<int> Evaluate(std::string_view text,const Lookup& lookup){auto p=Compile(text);return p?Evaluate(*p,lookup):std::nullopt;}
}
