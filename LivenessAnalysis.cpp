#include "LivenessAnalysis.h"

LivenessAnalysis::LivenessAnalysis() :
	FunctionPass(ID) {
}

LivenessAnalysis::~LivenessAnalysis() {
}

void LivenessAnalysis::db() {
	std::cout << "Here I come\n";
}

void LivenessAnalysis::setDefinitionVector(vector<Value *> allDef) {
	this->allDef = allDef;
	for (unsigned i = 0; i < allDef.size(); i++) {
		defIndex[allDef[i]] = i;
	}
}

bool LivenessAnalysis::runOnFunction(Function &F) {
	allDef.clear();
	defIndex.clear();

	//open file to write infomation
	std::string name = F.getParent()->getModuleIdentifier() + ".finfo";
	ofstream file((name.c_str()));
	raw_os_ostream ost(file);

	//find all definition and give them names (if they don't have one)
	for (Function::ArgumentListType::iterator it = F.getArgumentList().begin(); it
			!= F.getArgumentList().end(); it++) {
		allDef.push_back(it);
		defIndex[it] = allDef.size() - 1;
	}
	for (inst_iterator I = inst_begin(&F); I != inst_end(&F); ++I) {
		Instruction *isn = &*I;
		if (Utils::hasNewDef(isn)) {
			if (!isn->hasName())
				isn->setName(Utils::genId());
			allDef.push_back(isn);
			defIndex[isn] = allDef.size() - 1;
		}
	}

	//solve the problem
	map<Value*, DFAValue> &res = solve(&F);

	//output the result
	for (inst_iterator I = inst_begin(&F); I != inst_end(&F); ++I) {
		Instruction *isn = &*I;
		DFAValue &val = res[isn];
		//val.show();
		if (!isa<PHINode> (isn)) {
			ost << "{ ";
			for (unsigned i = 0; i < allDef.size(); i++)
				if (val.get(i) == 1)
					ost << allDef[i]->getNameStr() << " ";
			ost << "}\n";
		}
		ost << "\t" << *isn << "\n\n";
	}
	return false;
}

DFAValue LivenessAnalysis::transfer(Instruction *isn, DFAValue value) {
	DFAValue res = value;

	if (Utils::hasNewDef(isn)) { //Out - Def
		res.clear(defIndex[isn]);
	}

	if (isa<PHINode> (isn)) //just treat it a new defnition, the use of operands have been dealed in initilization
		return res;

	for (User::op_iterator it = isn->op_begin(); it != isn->op_end(); it++) {
		Value *val = *it;
		if (isa<Instruction> (val) || isa<Argument> (val)) { //now very clear here, why need this if
			res.set(defIndex[val]);
		}
	}
	return res;
}

void LivenessAnalysis::initialize(Function *fun) {
	DFAValue top = DFAValue(allDef.size());
	in.clear();
	out.clear();

	for (Function::iterator it = fun->getBasicBlockList().begin(); it
			!= fun->getBasicBlockList().end(); it++) {
		in[it] = top;
		out[it] = top;
	}

	for (inst_iterator I = inst_begin(fun); I != inst_end(fun); ++I) {
		PHINode *phi = dyn_cast<PHINode> (&*I);
		if (!phi)
			continue;
		for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
			Value *val = phi->getIncomingValue(i);
			BasicBlock *bb = NULL;
			if (Instruction *inst = dyn_cast<Instruction>(val)) {
				bb = inst->getParent();
			}
			if (isa<Argument> (val)) {
				bb = &fun->getEntryBlock(); //I think the conversion should be done in the begin or the end of the entry block
			}
			if (bb) {
				out[bb].set(defIndex[val]); //so the value must alive in the end of the block so it could do phi convert
			}
		}
	}
}

DFAValue LivenessAnalysis::meet(DFAValue a, DFAValue b) {
	return a | b;
}

bool LivenessAnalysis::isFoward() {
	return false;
}

bool LivenessAnalysis::isVaribleLiveIn(Value * var, Value* position) {
	int index = defIndex[var];
	DFAValue value = in[position];
	return value.get(index);
}

bool LivenessAnalysis::isVaribleLiveOut(Value * var, Value* position) {
	int index = defIndex[var];
	DFAValue value = out[position];
	return value.get(index);
}

char LivenessAnalysis::ID = 0;
RegisterPass<LivenessAnalysis> Y("liveness-dswp", "15745: liveness Analysis");
