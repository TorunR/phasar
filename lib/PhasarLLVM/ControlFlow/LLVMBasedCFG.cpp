/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * LLVMBasedCFG.cpp
 *
 *  Created on: 07.06.2017
 *      Author: philipp
 */
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <tuple>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#include <llvm/Support/Debug.h>
#include "phasar/Config/Configuration.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/Utils/LLVMShorthands.h"


using namespace std;
using namespace psr;

namespace psr {

const llvm::Function *
LLVMBasedCFG::getFunctionOf(const llvm::Instruction *Stmt) const {
  return Stmt->getFunction();
}

vector<const llvm::Instruction *>
LLVMBasedCFG::getPredsOf(const llvm::Instruction *I) const {
  vector<const llvm::Instruction *> Preds;
  if (I->getPrevNode()) {
    Preds.push_back(I->getPrevNode());
  }
  /*
   * If we do not have a predecessor yet, look for basic blocks which
   * lead to our instruction in question!
   */
  if (Preds.empty()) {
    for (const auto &BB : *I->getFunction()) {
      if (const llvm::Instruction *T = BB.getTerminator()) {
        for (unsigned Idx = 0; Idx < T->getNumSuccessors(); ++Idx) {
          if (&*T->getSuccessor(Idx)->begin() == I) {
            Preds.push_back(T);
          }
        }
      }
    }
  }
  return Preds;
}

vector<const llvm::Instruction *>
LLVMBasedCFG::getSuccsOf(const llvm::Instruction *I) const {
  vector<const llvm::Instruction *> Successors;
  if (I->getNextNode()) {
    Successors.push_back(I->getNextNode());
  }
  if (I->isTerminator()) {
    for (unsigned Idx = 0; Idx < I->getNumSuccessors(); ++Idx) {
      Successors.push_back(&*I->getSuccessor(Idx)->begin());
    }
  }
  return Successors;
}

vector<pair<const llvm::Instruction *, const llvm::Instruction *>>
LLVMBasedCFG::getAllControlFlowEdges(const llvm::Function *Fun) const {
  vector<pair<const llvm::Instruction *, const llvm::Instruction *>> Edges;
  for (const auto &BB : *Fun) {
    for (const auto &I : BB) {
      auto Successors = getSuccsOf(&I);
      for (const auto *Successor : Successors) {
        Edges.emplace_back(&I, Successor);
      }
    }
  }
  return Edges;
}

vector<const llvm::Instruction *>
LLVMBasedCFG::getAllInstructionsOf(const llvm::Function *Fun) const {
  vector<const llvm::Instruction *> Instructions;
  for (const auto &BB : *Fun) {
    for (const auto &I : BB) {
      Instructions.push_back(&I);
    }
  }
  return Instructions;
}

bool LLVMBasedCFG::isExitStmt(const llvm::Instruction *Stmt) const {
  return llvm::isa<llvm::ReturnInst>(Stmt);
}

bool LLVMBasedCFG::isStartPoint(const llvm::Instruction *Stmt) const {
  return (Stmt == &Stmt->getFunction()->front().front());
}

bool LLVMBasedCFG::isFieldLoad(const llvm::Instruction *Stmt) const {
  if (const auto *Load = llvm::dyn_cast<llvm::LoadInst>(Stmt)) {
    if (const auto *GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(
            Load->getPointerOperand())) {
      return true;
    }
  }
  return false;
}

bool LLVMBasedCFG::isFieldStore(const llvm::Instruction *Stmt) const {
  if (const auto *Store = llvm::dyn_cast<llvm::StoreInst>(Stmt)) {
    if (const auto *GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(
            Store->getPointerOperand())) {
      return true;
    }
  }
  return false;
}

bool LLVMBasedCFG::isFallThroughSuccessor(const llvm::Instruction *Stmt,
                                          const llvm::Instruction *Succ) const {
  // assert(false && "FallThrough not valid in LLVM IR");
  if (const auto *B = llvm::dyn_cast<llvm::BranchInst>(Stmt)) {
    if (B->isConditional()) {
      return &B->getSuccessor(1)->front() == Succ;
    } else {
      return &B->getSuccessor(0)->front() == Succ;
    }
  }
  return false;
}

bool LLVMBasedCFG::isBranchTarget(const llvm::Instruction *Stmt,
                                  const llvm::Instruction *Succ) const {
  if (Stmt->isTerminator()) {
    for (unsigned I = 0; I < Stmt->getNumSuccessors(); ++I) {
      if (&*Stmt->getSuccessor(I)->begin() == Succ) {
        return true;
      }
    }
  }
  return false;
}

string LLVMBasedCFG::getStatementId(const llvm::Instruction *Stmt) const {
  return llvm::cast<llvm::MDString>(
             Stmt->getMetadata(PhasarConfig::MetaDataKind())->getOperand(0))
      ->getString()
      .str();
}

string LLVMBasedCFG::getFunctionName(const llvm::Function *Fun) const {
  return Fun->getName().str();
}


bool hasSingleExitNode(const llvm::Function &fun) {
  bool foundOne = false;
  for (const auto &bb : fun) {
    // TODO: What is about throws
    if (llvm::isa<llvm::ReturnInst>(bb.getTerminator())) {
      if (foundOne) {
        return false;
      } else {
        foundOne = true;
      }
    }
  }
  return foundOne;
}

void getControlDependence() {}
using pathElem = pair<const llvm::BasicBlock *, const llvm::BasicBlock *>;
void getStrongControlDependence() {}
void getWeakControlDependence() {}

bool haveEmptyIntersection(const set<pathElem> &s1, const set<pathElem> &s2) {
  vector<pathElem> diff(s1.size());
  std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(), diff.begin());
  return !diff.empty() &&
         std::any_of(diff.begin(), diff.end(), [](pathElem &pathElem1) {
           return pathElem1 !=
                  make_pair<const llvm::BasicBlock *, const llvm::BasicBlock *>(
                      nullptr, nullptr);
         });
}

map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>>
LLVMBasedCFG::getNonTerminationSensitiveControlDependence(
    const llvm::Function &fun) {
  if (!fun.isDeclaration()) {
    deque<const llvm::BasicBlock *> wl;
    map<const llvm::BasicBlock *, map<const llvm::BasicBlock *, set<pathElem>>>
        maximalPaths;
    set<const llvm::BasicBlock *> condNodes;
    for (const auto &n : fun) {
      auto *term{n.getTerminator()};
      auto numSucc{term->getNumSuccessors()};
      if (numSucc > 1) {
        condNodes.insert(&n);
        for (unsigned int i = 0; i < numSucc; ++i) {
          auto *succ{term->getSuccessor(i)};
          wl.push_back(succ);
          auto &m = maximalPaths[succ];
          m[&n].emplace(&n, succ);
        }
      }
    }

    while (!wl.empty()) {
      auto n = wl.front();
      wl.pop_front();
      auto *term = n->getTerminator();
      auto numSucc = term->getNumSuccessors();
      if (numSucc == 1) {
        auto *m = term->getSuccessor(0);
        if (m != n) {
          for (auto *p : condNodes) {
            auto &s1{maximalPaths[n][p]};
            auto &s2{maximalPaths[m][p]};
            vector<pathElem> diff(s1.size());
            std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                diff.begin());
            if (haveEmptyIntersection(s1, s2)) {
              s2.insert(s1.begin(), s1.end());
              wl.push_back(m);
            }
          }
        }
      } else if (numSucc > 1) {
        for (auto &m : fun) {
          auto numSuccM = m.getTerminator()->getNumSuccessors();
          if (maximalPaths[&m][n].size() == numSuccM) {
            for (auto *p : condNodes) {
              if (p != n) {
                auto &s1{maximalPaths[n][p]};
                auto &s2{maximalPaths[&m][p]};
                vector<pathElem> diff(s1.size());
                std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                    diff.begin());
                if (haveEmptyIntersection(s1, s2)) {
                  s2.insert(s1.begin(), s1.end());
                  wl.push_back(&m);
                }
              }
            }
          }
        }
      }
    }

    for (auto &maximalPath : maximalPaths) {
      auto *p = maximalPath.first;
      for (auto &bla : maximalPath.second) {
        auto *n = bla.first;
        for (auto &m : bla.second) {
          llvm::dbgs() << "Path: " << n->getName() << " -> " << p->getName()
                       << " -> " << m.second->getName() << "\n";
        }
      }
    }
    map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>> cds;
    for (const auto &n : fun) {
      for (const auto *m : condNodes) {
        auto numPaths = maximalPaths[&n][m].size();
        auto numSucc = m->getTerminator()->getNumSuccessors();
        if (numPaths > 0 && numPaths < numSucc) {
          cds[m].insert(&n);
        }
      }
    }

    fun.viewCFG();

    for (auto &bb : cds) {
      bb.first->print(llvm::dbgs());
      llvm::dbgs()  << " " << bb.first->getValueName()->first()
                   << "\n";
      for (auto &bb2 : bb.second) {
        llvm::dbgs() << "\t";
        bb2->print(llvm::dbgs());
        llvm::dbgs() << " " << bb2->getValueName()->first() << "\n";
      }
    }

    return cds;
  }
  return std::map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>>();
}

map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>>
LLVMBasedCFG::getNonTerminationInsensitiveControlDependence(
    const llvm::Function &fun) {

  if (!fun.isDeclaration()) {
    deque<const llvm::BasicBlock *> wl;
    map<const llvm::BasicBlock *, map<const llvm::BasicBlock *, set<pathElem>>>
        maximalPaths;
    set<const llvm::BasicBlock *> condNodes;
    for (const auto &n : fun) {
      auto *term{n.getTerminator()};
      auto numSucc{term->getNumSuccessors()};
      if (numSucc > 1) {
        condNodes.insert(&n);
        for (unsigned int i = 0; i < numSucc; ++i) {
          auto *succ{term->getSuccessor(i)};
          wl.push_back(succ);
          auto &m = maximalPaths[succ];
          m[&n].emplace(&n, succ);
        }
      }
    }

    while (!wl.empty()) {
      auto n = wl.front();
      wl.pop_front();
      auto *term = n->getTerminator();
      auto numSucc = term->getNumSuccessors();
      if (numSucc == 1) {
        auto *m = term->getSuccessor(0);
        if (m != n) {
          for (auto *p : condNodes) {
            auto &s1{maximalPaths[n][p]};
            auto &s2{maximalPaths[m][p]};
            vector<pathElem> diff(s1.size());
            std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                diff.begin());
            if (haveEmptyIntersection(s1, s2)) {
              s2.insert(s1.begin(), s1.end());
              wl.push_back(m);
            }
          }
        }
      } else if (numSucc > 1) {
        for (auto &m : fun) {
          auto numSuccM = m.getTerminator()->getNumSuccessors();
          if (maximalPaths[&m][n].size() == numSuccM) {
            for (auto *p : condNodes) {
              if (p != n) {
                auto &s1{maximalPaths[n][p]};
                auto &s2{maximalPaths[&m][p]};
                vector<pathElem> diff(s1.size());
                std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                    diff.begin());
                if (haveEmptyIntersection(s1, s2)) {
                  s2.insert(s1.begin(), s1.end());
                  wl.push_back(&m);
                }
              }
            }
          }
        }
      }
      if (!maximalPaths[n][n].empty()) {
        for (unsigned i = 0; i < numSucc; ++i) {
          auto *m = term->getSuccessor(i);
          if (m != n) {
            auto &s1{maximalPaths[n][n]};
            auto &s2{maximalPaths[m][n]};
            vector<pathElem> diff(s1.size());
            std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                diff.begin());
            if (!diff.empty() &&
                std::any_of(diff.begin(), diff.end(), [](pathElem &pathElem1) {
                  return pathElem1 !=
                         make_pair<const llvm::BasicBlock *,
                                   const llvm::BasicBlock *>(nullptr, nullptr);
                })) {
              s2.insert(s1.begin(), s1.end());
              wl.push_back(m);
            }
          }
        }
      }
    }

    for (auto &maximalPath : maximalPaths) {
      auto *p = maximalPath.first;
      for (auto &bla : maximalPath.second) {
        auto *n = bla.first;
        for (auto &m : bla.second) {
          llvm::dbgs() << "Path: " << n->getName() << " -> " << p->getName()
                       << " -> " << m.second->getName() << "\n";
        }
      }
    }
    map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>> cds;
    for (const auto &n : fun) {
      for (const auto *m : condNodes) {
        auto numPaths = maximalPaths[&n][m].size();
        auto numSucc = m->getTerminator()->getNumSuccessors();
        if (numPaths > 0 && numPaths < numSucc) {
          cds[m].insert(&n);
        }
      }
    }

    //    fun.viewCFG();

    for (auto &bb : cds) {
      bb.first->print(llvm::dbgs());
      llvm::dbgs()  << " " << bb.first->getValueName()->first()
                    << "\n";
      for (auto &bb2 : bb.second) {
        llvm::dbgs() << "\t";
        bb2->print(llvm::dbgs());
        llvm::dbgs() << " " << bb2->getValueName()->first() << "\n";
      }
    }
    return cds;
  }
  return std::map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>>();
}

map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>>
LLVMBasedCFG::getDecisiveControlDependence(const llvm::Function &fun) {
  if (!fun.isDeclaration()) {
    deque<const llvm::BasicBlock *> wl;
    map<const llvm::BasicBlock *, map<const llvm::BasicBlock *, set<pathElem>>>
        S;
    set<const llvm::BasicBlock *> condNodes;
    for (const auto &n : fun) {
      auto *term{n.getTerminator()};
      auto numSucc{term->getNumSuccessors()};
      if (numSucc > 1) {
        condNodes.insert(&n);
        for (unsigned int i = 0; i < numSucc; ++i) {
          auto *succ{term->getSuccessor(i)};
          wl.push_back(succ);
          auto &m = S[succ];
          m[&n].emplace(&n, succ);
        }
      }
    }
    while (!wl.empty()) {
      auto n = wl.front();
      wl.pop_front();
      auto *term = n->getTerminator();
      auto numSuccs = term->getNumSuccessors();
      for (unsigned int i = 0; i < numSuccs; ++i) {
        auto *m = term->getSuccessor(i);
        for (auto *p : condNodes) {
          auto &s1{S[n][p]};
          auto &s2{S[m][p]};
          vector<pathElem> diff(s1.size());
          std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(),
                              diff.begin());
          if (haveEmptyIntersection(s1, s2)) {
            s2.insert(s1.begin(), s1.end());
            wl.push_back(m);
          }
        }
      }
    }
    map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>> cds{
        getNonTerminationSensitiveControlDependence(fun)};

    for (const auto &n : fun) {
      auto &cd{cds[&n]};
      for (auto it = cd.begin(); it != cd.end(); it++) {
        auto numSuccs = (*it)->getTerminator()->getNumSuccessors();
        if (S[&n][*it].size() == numSuccs) {
          auto temp = it;
          temp--;
          cd.erase(it);
          it = temp;
        }
      }
    }
    return cds;
  }
  return std::map<const llvm::BasicBlock *, set<const llvm::BasicBlock *>>();
}

void getOrderDependence() {}

void getStrongOrderDependence() {}

void getWeakOrderDependence() {}

void getDataSensitiveOrderDependence() {}


void LLVMBasedCFG::print(const llvm::Function *F, std::ostream &OS) const {
  OS << llvmIRToString(F);
}

nlohmann::json LLVMBasedCFG::getAsJson(const llvm::Function *F) const {
  return "";
}

} // namespace psr
