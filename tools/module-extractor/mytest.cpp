#include <iostream>
#include <map>
#include <utility>
#include <fstream>
#include <vector>
#include <llvm/IR/CFG.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <phasar/DB/ProjectIRDB.h>
#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSLinearConstantAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IDELinearConstantAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Solver/LLVMIFDSSolver.h>
#include <phasar/PhasarLLVM/IfdsIde/Solver/LLVMIDESolver.h>
#include <phasar/PhasarLLVM/Pointer/LLVMTypeHierarchy.h>
#include <phasar/Utils/LLVMIRToSrc.h>

#include <phasar/Utils/Logger.h>
#include <boost/filesystem/operations.hpp>

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
using namespace llvm;
using namespace std;
using namespace psr;

class ControlDependence {
public:
 ControlDependence(llvm::Instruction *control, llvm::Instruction *dependee): control(control),dependee(dependee){};
 llvm::Instruction *getControl() const {
     return  control;
 }
 llvm::Instruction *getDependee() const {
     return dependee;
 }
private:
 llvm::Instruction *control;
 llvm::Instruction *dependee;
};
// We want to produce a json file:
//[Function]
// Function {
// name: 
// cd: [
//  {control: Instruction
//  dependee: Instruction}
// ]
// Instruction Representation?
// 
// dd: [
// {def: Instruction 
// use: Instruction
// }
// ]
// 
// }

void writeMapToFile(map<llvm::Function*,pair<vector<ControlDependence*>,vector<llvm::Use*>>> &dependencies) {
    auto ec = error_code();
    llvm::raw_fd_ostream outFile(llvm::StringRef("results.json"), ec);
    outFile << "[";
    for (auto p = dependencies.begin();;){    
        outFile << "{\n \"name\": :\"" << p->first->getName().data() << "\",\n"
                << "\"cds\": [\n";
        for (auto cd=  p->second.first.begin();;){
            outFile << "{ \"control\":\"" 
            << *((*cd)->getControl()) 
            << "\",\"dependee\" : \""
            << *((*cd)->getDependee())
            << "\"}";
            cd++;
            if (cd == p->second.first.end()){
                break;
            } else {
                outFile << ",\n";
            }
        }
        outFile << "],\n\"dds\" : [\n";
        for (auto use = p->second.second.begin();;){
            outFile << "{ \"def\":\"";
            (*use)->get()->print(outFile);
            outFile << "\", \"use:\"" ;
            (*use)->getUser()->print(outFile); 
            outFile<<"}";
            use++;
            if (use == p->second.second.end()){
                break;
            } else {
                outFile << ",\n";          
            }
        }
        outFile << "]}";
        p++;
        if (p == dependencies.end()){        
            break;
        } else {
            outFile << ",";
        }
    }
    outFile << "]\n";
}

map<const DomTreeNodeBase<BasicBlock>*,vector<const DomTreeNodeBase<BasicBlock>*>> calculateDF(llvm::PostDominatorTree &PDT) {
    map<const DomTreeNodeBase<BasicBlock>*,vector<const DomTreeNodeBase<BasicBlock>*>> df;
    map<unsigned int, vector<const DomTreeNodeBase<BasicBlock>*>> tieredPDT;
    llvm::DomTreeNodeBase<llvm::BasicBlock>* root = PDT.getRootNode();
    llvm::SmallVector<llvm::DomTreeNodeBase<llvm::BasicBlock>*,5> WL;
    WL.push_back(root);

    while (!WL.empty()) {
      const DomTreeNodeBase<BasicBlock> *N = WL.pop_back_val();    
      tieredPDT[N->getLevel()].push_back(N);     
      for (auto c : N->getChildren()){
          WL.push_back(c);
      }
    }
    for (size_t i = tieredPDT.size()-1; i > 0; i--){                        
        for (auto node: tieredPDT[i]){
            for(auto s: successors(node->getBlock())){
                auto y =  PDT.getNode(s); 
                if (y->getIDom() != node){
                    df[node].push_back(y);
                }
            }
            for (auto c : node->getChildren()){            
               for (auto y : df[c]){                
                   if (y->getIDom() != c) {                       
                       df[node].push_back(y);
                   }
               }
            }
        }
    }    
    return df;
}

int main(int argc, const char **argv) {
    initializeLogger(true);
    ProjectIRDB DB({argv[1]}, IRDBOptions::WPA);
    auto result = map<llvm::Function*,pair<vector<ControlDependence*>,vector<llvm::Use*>>>();
    auto f = DB.getFunction("main");
    if (f){
        auto cds = vector<ControlDependence*>();
        auto dds = vector<llvm::Use*>();    
        llvm::PostDominatorTree PDT;
        PDT.recalculate(*f);
        auto df = calculateDF(PDT);
        PDT.print(llvm::dbgs());
        // f->print(llvm::dbgs());
        for (auto &bb : *f) {                 
            llvm::dbgs() << "====================================================\n";       
            auto node = PDT.getNode(&bb);            
            auto sv = llvm::SmallVector<llvm::BasicBlock*,5>();                    
            llvm::dbgs() << node << "\n";
            llvm::dbgs() << bb << "\n";
            for (auto cd: df[node]) {
                dbgs() <<"CD: " << cd << "\n";
                auto control = bb.getTerminator();
                for (auto &i : *(cd->getBlock())){
                   cds.push_back(new ControlDependence(control,&i));
                }                
            }
            for (auto &c : node->getChildren()) {
                llvm::dbgs() << "\t" <<c << "\n";
            }
            auto idom = node->getIDom();
            llvm::dbgs() <<"IDOM: "<< idom << "\n";
            // llvm::dbgs() << PDT.getNode(&bb) << "\n";
            for (auto &i : bb) {
                if (i.hasNUsesOrMore(1)) {
                    for (auto &u: i.uses()){
                       dds.push_back(&u);
                    }
                }
            }
            llvm::dbgs() << "====================================================\n";       
        }
        result[f] = make_pair(cds,dds);
    } else {
            llvm::dbgs() << "Did not find Function";
    }
    writeMapToFile(result);
    return 0;
}

