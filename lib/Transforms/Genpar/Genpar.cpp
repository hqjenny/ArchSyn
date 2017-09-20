#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/GenPar/Genpar.h"
#include "llvm/Transforms/BoostException.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpander.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/User.h"
#include <set>
#include <boost/lexical_cast.hpp>
using namespace llvm;

namespace llvm {

    /// \brief A passes to duplicate parallel loop body 
    struct Genpar : public ModulePass {
        public:
            static char ID;
            llvm::raw_ostream* out_c;
            bool printDA;
            Genpar() : ModulePass(ID) {}
            Genpar(llvm::raw_ostream& OS, bool DAInfo) : ModulePass(ID) {
                out_c= &OS;
                printDA = DAInfo;
            }

        bool runOnModule(Module &M) override {
           
            Module::FunctionListType &Fs = M.getFunctionList();

            for (Module::FunctionListType::iterator FI = Fs.begin(),
                    FE = Fs.end(); FI != FE; ++FI) {
                //Function &F = *FI;
                Function* F = &(*FI);

                // JENNY: If the parallel affine loop is detected in the function.
                // Currently the solution is to duplicate all the arguments, 
                // Should develop funtion to detect the arguments in the duped loops  
                

                //errs() <<(F->getName()) << '\n';
                // Pass in the number of splits 
                int NumSplits =2; 
                assert(NumSplits > 0);

                // Clone the functions and the arguments by NumSplit times 
                ValueToValueMapTy VMap;
                Function* NF = nullptr;  
                NF = cloneFunctionAndArguments(F, VMap, NumSplits);
                
                // Update the function call sites 
                updateCallSite(F, NF, NumSplits);
 
                //assert(NF != nullptr);
                DependenceAnalysis* DA = getAnalysisIfAvailable<DependenceAnalysis>();

                // Use this line to print out the info about memory reference independence 
                if (printDA) {
                    DA->print(errs(), NF->getParent());
                    return false;
                }

                LoopInfo* LI = &getAnalysis<LoopInfo>(*NF);
 
                if (!LI){
                    errs() << NF->getName() << "\t";
                    errs() <<"LoopInfo not available."<<"\n";
                }

                // Split the loop in to NumSplits
                //ScalarEvolution *se = getAnalysisIfAvailable<ScalarEvolution>();
                ValueToValueMapTy LoopVMap;

                splitLoop(DA, LI, NF, NumSplits, LoopVMap);

           }

            return true;
        }

        // Split loop and the helper functions
        void splitLoop(DependenceAnalysis * DA, LoopInfo* LI, \
                Function* F, int NumSplits, ValueToValueMapTy& VMap);
        void cloneLoop(Function* F, Loop* CurLoop, ValueToValueMapTy& VMap, int NumSplits);
        void dfsBackward(std::vector<Value*>& storage, PHINode*& initPhi, \
                Instruction* curHop, Loop* CurLoop);
        CmpInst* getExitCmp(Loop* CurLoop);
        BasicBlock* getTakenTarget(BasicBlock* exitingBB); 

        // Clone function arguments and the helper functions
        Function* cloneFunctionAndArguments(Function* F, ValueToValueMapTy& VMap, int NumSplits);


        void replaceArgs(Function* NF, int NumSplits, int SplitIdx, BasicBlock* BB);
        void replaceAllUsesOfWithIn(Argument *I, Argument *J, BasicBlock *BB);

        void updateCallSite (Function* F, Function* NF, int NumSplits);             


        void printInstructionTree(Instruction* m, std::vector<Instruction*>& storage);
        virtual void getAnalysisUsage(AnalysisUsage &AU) const override
        {
            AU.addRequired<LoopInfo>();
            AU.addRequired<DependenceAnalysis>();
            AU.addRequired<ScalarEvolution>();
        }
        
    };

    // Clone the BBs in a loop
    void Genpar::cloneLoop(Function* F, Loop* CurLoop, ValueToValueMapTy& VMap, int NumSplits) {
        BasicBlock* OrigHead = CurLoop->getHeader();
        BasicBlock* OrigExit = CurLoop->getExitingBlock();
        char const* NameSuffix = "dup";
        std::vector<BasicBlock*> dupedBBs;
        for (auto BI = CurLoop->getBlocks().begin(); \
            BI != CurLoop->getBlocks().end(); BI++) {
            const BasicBlock* BB = *BI;

            // Create a new basic block and copy instructions into it!
            BasicBlock *CBB = CloneBasicBlock(BB, VMap, NameSuffix, OrigHead->getParent());
            replaceArgs(F, NumSplits, 1, CBB); 
            // Add basic block mapping.
            // Mapping: Old BB - New BB 
            VMap[BB] = CBB;
            dupedBBs.push_back(CBB);
        }
        // We will add instructions to compute new bounds -- right before the header
        // of the original loop

        Value* newHead = VMap[OrigHead];

        // the original exiting block would branch to outside of the
        // loop, what ever goes out of the loop, we redirect it to the new head
        // FIXME:we make assumption about the loop structure which is only true
        // for our benchmark
        // TODO Make sure there is no branch outside of loop during the parallelism analysis
        TerminatorInst* exitingIns = OrigExit->getTerminator();
        int numSuc = exitingIns->getNumSuccessors();
        for (int sucInd = 0; sucInd<numSuc; sucInd++) {
            BasicBlock* curSuc = exitingIns->getSuccessor(sucInd);
            // the successor is not part of the loop
            if(VMap.find(curSuc)==VMap.end()) {
                exitingIns->setSuccessor(sucInd,dyn_cast<BasicBlock>(newHead));
            }
        }
        //errs()<<"Terminator "<<*exitingIns<<" XXXXXXXXXXXXXXXXXXXXXXXX\n";
        // Iterate through the duplicated basic block
        for (auto dupBBIter = dupedBBs.begin(); dupBBIter!= dupedBBs.end(); dupBBIter++) {
            BasicBlock* curBB = *dupBBIter;

            for (auto dupInsIter = curBB->begin(); dupInsIter!= curBB->end(); dupInsIter++) {
                Instruction* curIns = dyn_cast<Instruction>(dupInsIter);
                PHINode* curPhi = dyn_cast<PHINode>(dupInsIter);

                // Map the incomping block of the new PhiNode to 
                if (curPhi) {
                    int numIncomingEdges = curPhi->getNumIncomingValues();

                    for (int i=0; i < numIncomingEdges; i++) {
                        // Map the phi to new duped incomingblocks 
                        BasicBlock* incomingBlock = curPhi->getIncomingBlock(i);
                        if (VMap.find(incomingBlock)!=VMap.end()) {
                            BasicBlock* newIncomingBlock = dyn_cast<BasicBlock>(VMap[incomingBlock]);
                            curPhi->setIncomingBlock(i, newIncomingBlock);

                        // If the incoming block does not have a counter part in the original block,  
                        // Assume it is from the loop predecessor, so direct it to the exit block of the original loop exit 
                        } else {
                            curPhi->setIncomingBlock(i, OrigExit);
                        }

                    }
                }
                
                // Map the old operands to the updated operands
                int numOperands = curIns->getNumOperands();
                for (int opInd = 0; opInd < numOperands; opInd++) {
                    Value* originalOperand = curIns->getOperand(opInd);

                    if (VMap.find(originalOperand)!=VMap.end()) {
                        Value* newOperand = VMap[originalOperand];
                        curIns->setOperand(opInd,newOperand);
                    }
                }
            }
        }
        
        errs()<<"=========================================end of clone=========\n";
    }

    void Genpar::printInstructionTree(Instruction* m, std::vector<Instruction*>& storage) {
        if (std::find(storage.begin(),storage.end(),m)!=storage.end())
            return;
        storage.push_back(m);
        for(unsigned int opInd = 0; opInd < m->getNumOperands(); opInd++) {
            Instruction* curOperandIns = dyn_cast<Instruction>(m->getOperand(opInd));

            if(curOperandIns) {
                errs()<<*curOperandIns<<"\n";
                printInstructionTree(curOperandIns, storage);
            }
            errs()<<"-----------------------\n";
        }
    }

    // Depth first search for the PHINode
    void Genpar::dfsBackward(std::vector<Value*>& storage, PHINode*& initPhi, Instruction* curHop, Loop* CurLoop=NULL) {
        if (std::find(storage.begin(),storage.end(),curHop)!=storage.end())
            return;
        storage.push_back(curHop);

        if (dyn_cast<PHINode>(curHop)) {
            // if phi is having one incoming edge from outside the loop
            // and the value is from outside the loop
            // then this is the initPhi
            // FIXME:Again we make assumptions about loop structure
            PHINode* curPhi = dyn_cast<PHINode>(curHop);

            if (CurLoop && CurLoop->getHeader() == curPhi->getParent() )
                initPhi = curPhi;
        }
        for (unsigned int i=0; i<curHop->getNumOperands(); i++) {
            Value* curOperand = curHop->getOperand(i);
            Instruction* curIns = dyn_cast<Instruction>(curOperand);

            if (curIns) {
                dfsBackward(storage, initPhi, curIns, CurLoop);
            }
        }
    }


    CmpInst* Genpar::getExitCmp(Loop* CurLoop) {
        BasicBlock* lastBlock = CurLoop->getExitingBlock();
        BranchInst* term = dyn_cast<BranchInst>(lastBlock->getTerminator());
        assert(term->isConditional());
        CmpInst* compareCondition = dyn_cast<CmpInst>(term->getOperand(0));
        return compareCondition;
    }

    BasicBlock* Genpar::getTakenTarget(BasicBlock* exitingBB) {
        BranchInst* term = dyn_cast<BranchInst>(exitingBB->getTerminator());
        // 0th is the condition, 1st is the taken
        Value* taken = term->getOperand(2);
        return dyn_cast<BasicBlock>(taken);
    }

    void Genpar::replaceAllUsesOfWithIn(Argument *I, Argument *J, BasicBlock *BB) {

        for (Argument::user_iterator UI = I->user_begin(), UE = I->user_end(); UI != UE;) {
            Use &TheUse = UI.getUse();
            ++UI;

            //errs() << "replace use!\n"; 
            if (Instruction *II = dyn_cast<Instruction>(TheUse.getUser())) {
                                    //for (Argument::const_user_iterator UI = I -> user_begin(), UE = I -> user_end(); UI != UE; ++UI){

                                        //if (Instruction *Inst = dyn_cast<Instruction>(*UI)) {
                                        //errs() << "F is used in instruction:\n";
                                        //errs() << *Inst << "\n";
                                        //}
                                    //errs() << "Inst:" << *II << "\n" ;
                                    //}


                if (BB == II->getParent()){
                    //errs() << "replace!\n"; 
                    II->replaceUsesOfWith((Value*)I, (Value*)J);
                }
            }
        }
    }

   
    void Genpar::replaceArgs(Function* NF, int NumSplits, int SplitIdx, BasicBlock* BB) {
        assert(SplitIdx > 0);
        // Replace Arguments 
        for(Function::arg_iterator I = NF->arg_begin(), E = NF->arg_end(); I!=E;){
            //int count = 0;
            //for (Argument::user_iterator UI = I -> user_begin(), UE = I -> user_end(); UI != UE; ++UI){

                //count++;
                //if (Instruction *Inst = dyn_cast<Instruction>(*UI)) {
                //errs() << "F is used in instruction:\n";
                //errs() << *Inst << "\n";
            //}

            Argument* OrigArg = &(*I);
            int LocalIdx = 0; 
            while (LocalIdx != SplitIdx) {
                I++;
                LocalIdx++;   
            }
            Argument* NewArg = &(*I);
            while (LocalIdx < NumSplits) {
                I++;
                LocalIdx++;
            }
        
            //errs() << "Arg Name:" << OrigArg->getName() << " To " <<  NewArg -> getName() << "\n";
            //errs() << "Count Use: " << count << "\n";

            replaceAllUsesOfWithIn(OrigArg, NewArg, BB);
        }
    }


    void Genpar::splitLoop(DependenceAnalysis * DA, LoopInfo* LI, Function* F, int NumSplits, ValueToValueMapTy& VMap) {

        // This part duplicate the outermost loop
        // and change the bound
        // Iterate throught differents levels of the loop nest
        for (auto I = LI->begin(), E = LI->end(); I != E; I++) {
            Loop* CurLoop = *I;

            if (CurLoop->getLoopDepth() == 1) {
                // This is the outer most loop
                // find the loop bound first
                cloneLoop(F, CurLoop, VMap, NumSplits);
                Value* lowerBound;
                Value* upperBound;
                CmpInst* exitCmp = getExitCmp(CurLoop);

                BasicBlock* takenSuccessor = getTakenTarget(CurLoop->getExitingBlock());

                // Taken Successor is not in the loop body 
                bool exitOnTaken = (CurLoop->getBlocks().end()==std::find(CurLoop->getBlocks().begin(),
                                             CurLoop->getBlocks().end(),
                                             takenSuccessor));

               
                // Return ICMP FCMP predicate value, compare to constant
                CmpInst::Predicate p = exitCmp->getPredicate();

                // TODO Add unsigned greater than UGT?
                if (p==CmpInst::ICMP_SGT && exitOnTaken) {
                    // When we exit with SGT flag being true
                    // the upper bound is the second operand
                    upperBound = exitCmp->getOperand(1);
                    // We then need to find phiNode which defines the lower bound
                    // we trace back where the other operand comes from
                    // it must contains PHINode and one of the PHINode is taking in
                    // value from outside the loop -- that shall be the lower bound
                    std::vector<Value*> nodeChain;
                    PHINode* initializationPhi=NULL;


                    // TODO Look at getSmallConstantTripCount to see if there is a better way to find the loop bound
                    dfsBackward(nodeChain, initializationPhi, exitCmp, CurLoop);
                    if(initializationPhi!=NULL)
                        errs()<<"init phi"<<*initializationPhi<<"\n";
                    else
                        assert(false && "cannot find init phi");

                    int numIncomingBlocks = initializationPhi->getNumIncomingValues();
                    BasicBlock* beforeLoop=NULL;
                    int lowerBoundInd = -1;

                    // Get the lower bound from the incoming blocks outside of the loop body
                    for(int bInd = 0; bInd < numIncomingBlocks; bInd++) {
                        BasicBlock* pred = initializationPhi->getIncomingBlock(bInd);
                        if(CurLoop->getBlocks().end()==std::find(CurLoop->getBlocks().begin(),
                                             CurLoop->getBlocks().end(),
                                             pred)) {
                            lowerBound = initializationPhi->getIncomingValueForBlock(pred);
                            lowerBoundInd = bInd;
                            beforeLoop = pred;
                        }
                    }
                    assert(beforeLoop && "cannot find BB before loop for assignment insertion\n");
                    // Now both upper and lower bound are found
                    // compute a new value by shifting upperbound to the right by 1
                    // this be the upperbound/lowerbound for the first/second dup
                    // the original upperbound becomes the upper bound for the second dup
                    TerminatorInst* beforeLoopTerm = beforeLoop->getTerminator();
                    IRBuilder<> builder(beforeLoop);
                    

                    // LocalRange = Range / NumSplit
                    //Value* Range = builder.CreateSub(upperBound, lowerBound);
                    //Constant* NumSplitsConst = ConstantInt::get(Range->getType(), NumSplits);
                    // Use Shift instead if NumSplits is power of 2 
                    //Value* LocalRange = build.CreateUDiv(Range, NumSplitsConst);
                    

                    Value* halfUpper = builder.CreateLShr(upperBound,1);
                    Constant* const1 = ConstantInt::get(halfUpper->getType(),1);

                    Value* halfUpperP1 = builder.CreateAdd(halfUpper,const1);
                    Instruction* halfUpperIns = dyn_cast<Instruction>(halfUpper);
                    Instruction* halfUpperP1Ins =  dyn_cast<Instruction>(halfUpperP1);

                    halfUpperIns->moveBefore(beforeLoopTerm);
                    halfUpperP1Ins->moveBefore(beforeLoopTerm);
                    // set new upper bound for first copy
                    exitCmp->setOperand(1,halfUpper);
                    // set new lower bound for second copy
                    Value* newInitPhi = VMap[initializationPhi];
                    PHINode* newPhi = dyn_cast<PHINode>(newInitPhi);
                    newPhi->setIncomingValue(lowerBoundInd,halfUpperP1);
                }
            } else {
                //FIXME: other scenarios for loop bound determination
                assert(false && "currently not implemented, go ahead and add the logic");
            }
        }
    }

    // ? Replace the argument used in the function bodyreplaceDominatedUsesWith ?
    void Genpar::updateCallSite (Function* F, Function* NF, int NumSplits) {             
        SmallVector<Value*, 16> Args;
        while (!F->use_empty()) {
            llvm::CallSite CS(F->user_back());
            assert(CS.getCalledFunction() == F);
            Instruction *Call = CS.getInstruction();
            const AttributeSet &CallPAL = CS.getAttributes();

            // Add construct the call site to the new function
            CallSite::arg_iterator AI = CS.arg_begin();
            for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
                I != E; ++I, ++AI) {
                // Duplicate each arguments 
                for (int Idx = 1; Idx < NumSplits; Idx++) {
                    Args.push_back(*AI);          // Unmodified argument
                }
            }

            // Add new call instruction
            Instruction *NewCall = NULL;

            // Replace old call with new call
            const AttributeSet &CallAttr = CS.getAttributes();
            if (InvokeInst *II = dyn_cast< InvokeInst >(Call)) {
                NewCall = InvokeInst::Create
                    (NF,
                    II->getNormalDest(),
                    II->getUnwindDest(),
                    Args, "", Call);
                InvokeInst *inv = cast< InvokeInst >(NewCall);
                inv->setCallingConv(CS.getCallingConv());
                inv->setAttributes(CallAttr);

            } else {
                NewCall = CallInst::Create
                    (NF, Args, "", Call);
                CallInst *CI = cast< CallInst >(NewCall);
                CI->setCallingConv(CS.getCallingConv());
                CI->setAttributes(CallAttr);

                if (CI->isTailCall())
                    CI->setTailCall();
            }

            NewCall->setDebugLoc
                (Call->getDebugLoc());

            if (!Call->use_empty()) {
                Call->replaceAllUsesWith(NewCall);
            }
            NewCall->takeName(Call);
            Call->eraseFromParent();
        }
    }

    Function* Genpar::cloneFunctionAndArguments(Function* F, ValueToValueMapTy& VMap, int NumSplits) {
        // Copy to a new function with duplicated args
        // Need to update Function Types 
        std::vector<llvm::Type *> ArgTypes;
        //FunctionType *ft = F->getFunctionType();
        for (Function::const_arg_iterator I = F->arg_begin(), E=F->arg_end(); I!=E; ++I) {
            // Duplicate the arguments 
            for(int Idx = 0; Idx < NumSplits; Idx++)
                ArgTypes.push_back(I->getType());
        }

        // Refer to CloneFunction.cpp
        FunctionType* FTy = FunctionType::get(F->getFunctionType()->getReturnType(), ArgTypes, F->getFunctionType()->isVarArg());

        // Gather the arguments that needs to be duplicated 
        Function* NF = Function::Create(FTy, F->getLinkage(), F->getName());

        Function::arg_iterator DestI = NF->arg_begin();
        for (Function::const_arg_iterator I = F->arg_begin(), E=F->arg_end(); I!=E; ++I) {
           (DestI) -> setName(I->getName()); 
            VMap[I] = DestI++;
            for (int Idx = 1; Idx < NumSplits; Idx++) 
                (DestI++) -> setName(I->getName() +"_" + std::to_string(Idx)); 
        }
            
        // Clone argument attributes 
        DestI = NF -> arg_begin();
        AttributeSet OldAttrs = F -> getAttributes();
        errs() << "Number of Attributes" << std::to_string(OldAttrs.getNumSlots()) << "\n";
        errs() << "Attributes:" << OldAttrs.getAsString(0, false) << "\n";

        for (const Argument &OldArg : F->args()) {
            // errs() << "Arg Name " << OldArg.getName() << std::to_string(OldArg.getArgNo()) << "\n";
            AttributeSet attrs =
                OldAttrs.getParamAttributes(OldArg.getArgNo() + 1); // 
            if(attrs.getNumSlots() > 0){
                for (int Idx = 1; Idx < NumSplits; Idx++) 
                    (DestI++) -> addAttr(attrs);
            }
        }

        // We can pass in VMap[I] = DestI++; so the CloneFunctionInto will clone the attributes, but it is only able to update one new argument
        bool ModuleLevelChanges = false; 
        // Clone the function bodies
        SmallVector<ReturnInst*,8> Returns;
        CloneFunctionInto(NF, F, VMap, ModuleLevelChanges, Returns); //ModuleLevelChanges=false, not sure if we should set it to true
        //if (ModuleLevelChanges) 
        //    CloneDebugInfoMetadata(NF, F, VMap);

        // Refer to ArgumentPromotion.cpp 
        // Change the module symbol lookup table? 
        //NF->copyAttributesFrom(F); is done in CloneFunctionInto
        // TODO check the attributes in the ir file
        F->getParent()->getFunctionList().insert(F, NF);
        NF->takeName(F);
        return NF;
    }


}

char Genpar::ID = 0;

static RegisterPass<Genpar> X("gen-par", "generate par -- generate the parallelizable IR");
ModulePass *llvm::createGenParPass(llvm::raw_ostream &OS, bool DAInfo)
{
    struct Genpar* gp = new Genpar(OS, DAInfo);
    return gp;
}


