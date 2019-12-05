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

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>

#include <llvm/Support/Debug.h>
#include <phasar/Config/Configuration.h>
#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h>

using namespace std;
using namespace psr;

namespace psr {

const llvm::Function *LLVMBasedCFG::getMethodOf(const llvm::Instruction *stmt) {
  return stmt->getFunction();
}

vector<const llvm::Instruction *>
LLVMBasedCFG::getPredsOf(const llvm::Instruction *I) {
  vector<const llvm::Instruction *> Preds;
  if (I->getPrevNode()) {
    Preds.push_back(I->getPrevNode());
  }
  /*
   * If we do not have a predecessor yet, look for basic blocks which
   * lead to our instruction in question!
   */
  if (Preds.empty()) {
    for (auto &BB : *I->getFunction()) {
      if (const llvm::Instruction *T = BB.getTerminator()) {
        for (unsigned i = 0; i < T->getNumSuccessors(); ++i) {
          if (&*T->getSuccessor(i)->begin() == I) {
            Preds.push_back(T);
          }
        }
      }
    }
  }
  return Preds;
}

vector<const llvm::Instruction *>
LLVMBasedCFG::getSuccsOf(const llvm::Instruction *I) {
  vector<const llvm::Instruction *> Successors;
  if (I->getNextNode()) {
    Successors.push_back(I->getNextNode());
  }
  if (I->isTerminator()) {
    for (unsigned i = 0; i < I->getNumSuccessors(); ++i) {
      Successors.push_back(&*I->getSuccessor(i)->begin());
    }
  }
  return Successors;
}

vector<pair<const llvm::Instruction *, const llvm::Instruction *>>
LLVMBasedCFG::getAllControlFlowEdges(const llvm::Function *fun) {
  vector<pair<const llvm::Instruction *, const llvm::Instruction *>> Edges;
  for (auto &BB : *fun) {
    for (auto &I : BB) {
      auto Successors = getSuccsOf(&I);
      for (auto Successor : Successors) {
        Edges.push_back(make_pair(&I, Successor));
      }
    }
  }
  return Edges;
}

vector<const llvm::Instruction *>
LLVMBasedCFG::getAllInstructionsOf(const llvm::Function *fun) {
  vector<const llvm::Instruction *> Instructions;
  for (auto &BB : *fun) {
    for (auto &I : BB) {
      Instructions.push_back(&I);
    }
  }
  return Instructions;
}

bool LLVMBasedCFG::isExitStmt(const llvm::Instruction *stmt) {
  return llvm::isa<llvm::ReturnInst>(stmt);
}

bool LLVMBasedCFG::isStartPoint(const llvm::Instruction *stmt) {
  return (stmt == &stmt->getFunction()->front().front());
}

bool LLVMBasedCFG::isFieldLoad(const llvm::Instruction *stmt) {
  if (auto Load = llvm::dyn_cast<llvm::LoadInst>(stmt)) {
    if (auto GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(
            Load->getPointerOperand())) {
      return true;
    }
  }
  return false;
}

bool LLVMBasedCFG::isFieldStore(const llvm::Instruction *stmt) {
  if (auto Store = llvm::dyn_cast<llvm::StoreInst>(stmt)) {
    if (auto GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(
            Store->getPointerOperand())) {
      return true;
    }
  }
  return false;
}

bool LLVMBasedCFG::isFallThroughSuccessor(const llvm::Instruction *stmt,
                                          const llvm::Instruction *succ) {
  // assert(false && "FallThrough not valid in LLVM IR");
  if (const llvm::BranchInst *B = llvm::dyn_cast<llvm::BranchInst>(stmt)) {
    if (B->isConditional()) {
      return &B->getSuccessor(1)->front() == succ;
    } else {
      return &B->getSuccessor(0)->front() == succ;
    }
  }
  return false;
}

bool LLVMBasedCFG::isBranchTarget(const llvm::Instruction *stmt,
                                  const llvm::Instruction *succ) {
  if (stmt->isTerminator()) {
    for (unsigned i = 0; i < stmt->getNumSuccessors(); ++i) {
      if (&*stmt->getSuccessor(i)->begin() == succ) {
        return true;
      }
    }
  }
  return false;
}

string LLVMBasedCFG::getStatementId(const llvm::Instruction *stmt) {
  return llvm::cast<llvm::MDString>(
             stmt->getMetadata(PhasarConfig::MetaDataKind())->getOperand(0))
      ->getString()
      .str();
}

string LLVMBasedCFG::getMethodName(const llvm::Function *fun) {
  return fun->getName().str();
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
      llvm::dbgs() << bb.first << " " << bb.first->getValueName()->first()
                   << "\n";
      for (auto &bb2 : bb.second)
        llvm::dbgs() << "\t" << bb2 << " " << bb2->getValueName()->first()
                     << "\n";
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
      llvm::dbgs() << bb.first << " " << bb.first->getValueName()->first()
                   << "\n";
      for (auto &bb2 : bb.second)
        llvm::dbgs() << "\t" << bb2 << " " << bb2->getValueName()->first()
                     << "\n";
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

} // namespace psr
