#include <assert.h>
#include "Jitter.h"
#include "PtrMacro.h"
#include "placeholder_def.h"

using namespace std;
using namespace Jitter;

CJitter::CJitter(CCodeGen* codeGen)
: m_nBlockStarted(false)
, m_codeGen(codeGen)
{

}

CJitter::~CJitter()
{
	delete m_codeGen;
}

void CJitter::SetStream(Framework::CStream* stream)
{
	m_codeGen->SetStream(stream);
}

void CJitter::Begin()
{
	assert(m_nBlockStarted == false);
	m_nBlockStarted = true;
	m_nextTemporary = 1;
	m_nextBlockId = 1;
	m_basicBlocks.clear();

	uint32 blockId = CreateBlock();
	m_currentBlock = GetBlock(blockId);
}

void CJitter::End()
{
	assert(m_nBlockStarted == true);
	m_nBlockStarted = false;

	Compile();
}

bool CJitter::IsStackEmpty()
{
    return m_Shadow.GetCount() == 0;
}

CONDITION CJitter::GetReverseCondition(CONDITION condition)
{
	switch(condition)
	{
	case CONDITION_EQ:
		return CONDITION_NE;
		break;
	default:
		assert(0);
		break;
	}
	throw std::exception();
}

void CJitter::BeginIf(CONDITION condition)
{
	uint32 nextBlockId = CreateBlock();
	uint32 jumpBlockId = CreateBlock();

	STATEMENT statement;
	statement.op			= OP_CONDJMP;
	statement.src2			= MakeSymbolRef(m_Shadow.Pull());
	statement.src1			= MakeSymbolRef(m_Shadow.Pull());
	statement.jmpCondition	= GetReverseCondition(condition);
	statement.jmpBlock		= jumpBlockId;
	InsertStatement(statement);

	assert(m_Shadow.GetCount() == 0);

	m_currentBlock = GetBlock(nextBlockId);
	m_IfStack.Push(jumpBlockId);
}

void CJitter::Else()
{
	assert(m_IfStack.GetCount() > 0);
	assert(m_Shadow.GetCount() == 0);

	uint32 nextBlockId = m_IfStack.Pull();
	uint32 jumpBlockId = CreateBlock();

	STATEMENT statement;
	statement.op			= OP_JMP;
	statement.jmpBlock		= jumpBlockId;
	InsertStatement(statement);

	m_currentBlock = GetBlock(nextBlockId);
	m_IfStack.Push(jumpBlockId);
}

void CJitter::EndIf()
{
	assert(m_IfStack.GetCount() > 0);
	assert(m_Shadow.GetCount() == 0);

	uint32 nextBlockId = m_IfStack.Pull();
	m_currentBlock = GetBlock(nextBlockId);
}

void CJitter::PushCtx()
{
	m_Shadow.Push(MakeSymbol(SYM_CONTEXT, 0));
}

void CJitter::PushCst(uint32 nValue)
{
	m_Shadow.Push(MakeSymbol(SYM_CONSTANT, nValue));
}

void CJitter::PushRef(void* pAddr)
{
	size_t addrSize = sizeof(pAddr);
	if(addrSize == 4)
	{
		m_Shadow.Push(MakeSymbol(SYM_CONSTANT, *reinterpret_cast<uint32*>(&pAddr)));
	}
	else if(addrSize == 8)
	{
		m_Shadow.Push(MakeConstant64(*reinterpret_cast<uint64*>(&pAddr)));
	}
	else
	{
		assert(0);
	}
}

void CJitter::PushRel(size_t nOffset)
{
	m_Shadow.Push(MakeSymbol(SYM_RELATIVE, static_cast<uint32>(nOffset)));
}

void CJitter::PushIdx(unsigned int nIndex)
{
	m_Shadow.Push(m_Shadow.GetAt(nIndex));
}

void CJitter::PushTop()
{
	PushIdx(0);
}

void CJitter::PullRel(size_t nOffset)
{
	STATEMENT statement;
	statement.op		= OP_MOV;
	statement.src1		= MakeSymbolRef(m_Shadow.Pull());
	statement.dst		= MakeSymbolRef(MakeSymbol(SYM_RELATIVE, nOffset));
	InsertStatement(statement);

	assert(GetSymbolSize(statement.src1) == GetSymbolSize(statement.dst));
}

void CJitter::PullTop()
{
	m_Shadow.Pull();
	m_Shadow.Pull();
}

void CJitter::Swap()
{
	throw std::exception();
}

void CJitter::Add()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_ADD;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::And()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_AND;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Call(void* pFunc, unsigned int nParamCount, bool nKeepRet)
{
	for(unsigned int i = 0; i < nParamCount; i++)
	{
		STATEMENT paramStatement;
		paramStatement.src1	= MakeSymbolRef(m_Shadow.Pull());
		paramStatement.op	= OP_PARAM;
		InsertStatement(paramStatement);
	}

	size_t addrSize = sizeof(pFunc);

	STATEMENT callStatement;
	if(addrSize == 4)
	{
		callStatement.src1 = MakeSymbolRef(MakeSymbol(SYM_CONSTANT, reinterpret_cast<uint32>(pFunc)));
	}
	else if(addrSize == 8)
	{
		callStatement.src1 = MakeSymbolRef(MakeConstant64(reinterpret_cast<uint64>(pFunc)));
	}
	else
	{
		assert(0);
	}
	callStatement.src2 = MakeSymbolRef(MakeSymbol(SYM_CONSTANT, nParamCount));
	callStatement.op = OP_CALL;
	InsertStatement(callStatement);

	if(nKeepRet)
	{
		SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

		STATEMENT returnStatement;
		returnStatement.dst = MakeSymbolRef(tempSym);
		returnStatement.op	= OP_RETVAL;
		InsertStatement(returnStatement);

		m_Shadow.Push(tempSym);
	}
}

void CJitter::Cmp(CONDITION condition)
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op			= OP_CMP;
	statement.src2			= MakeSymbolRef(m_Shadow.Pull());
	statement.src1			= MakeSymbolRef(m_Shadow.Pull());
	statement.jmpCondition	= condition;
	statement.dst			= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Div()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_DIV;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::DivS()
{
	throw std::exception();
}

void CJitter::Lookup(uint32* table)
{
	throw std::exception();
}

void CJitter::Lzc()
{
	throw std::exception();
}

void CJitter::Mult()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY64, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_MUL;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::MultS()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY64, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_MULS;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Not()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_NOT;
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Or()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_OR;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::SignExt()
{
	throw std::exception();
}

void CJitter::SignExt8()
{
	Shl(24);
	Sra(24);
}

void CJitter::SignExt16()
{
	Shl(16);
	Sra(16);
}

void CJitter::Shl()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SLL;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Shl(uint8 nAmount)
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SLL;
	statement.src2	= MakeSymbolRef(MakeSymbol(SYM_CONSTANT, nAmount));
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Sra()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SRA;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Sra(uint8 nAmount)
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SRA;
	statement.src2	= MakeSymbolRef(MakeSymbol(SYM_CONSTANT, nAmount));
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Srl()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SRL;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Srl(uint8 nAmount)
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SRL;
	statement.src2	= MakeSymbolRef(MakeSymbol(SYM_CONSTANT, nAmount));
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Sub()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_SUB;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::Xor()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_XOR;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

//64-bits
//------------------------------------------------
void CJitter::PushRel64(size_t offset)
{
	m_Shadow.Push(MakeSymbol(SYM_RELATIVE64, static_cast<uint32>(offset)));
}

void CJitter::PullRel64(size_t offset)
{
	STATEMENT statement;
	statement.op		= OP_MOV;
	statement.src1		= MakeSymbolRef(m_Shadow.Pull());
	statement.dst		= MakeSymbolRef(MakeSymbol(SYM_RELATIVE64, offset));
	InsertStatement(statement);

	assert(GetSymbolSize(statement.src1) == GetSymbolSize(statement.dst));
}

void CJitter::ExtLow64()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_EXTLOW64;
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	assert(GetSymbolSize(statement.src1) == 8);

	m_Shadow.Push(tempSym);
}

void CJitter::ExtHigh64()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_EXTHIGH64;
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	assert(GetSymbolSize(statement.src1) == 8);

	m_Shadow.Push(tempSym);
}

void CJitter::Add64()
{
	SymbolPtr tempSym = MakeSymbol(SYM_TEMPORARY64, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_ADD64;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::And64()
{
	throw std::exception();
}

void CJitter::Cmp64(CONDITION nCondition)
{
	throw std::exception();
}

void CJitter::Sub64()
{
	throw std::exception();
}

void CJitter::Srl64()
{
	throw std::exception();
}

void CJitter::Srl64(uint8 nAmount)
{
	throw std::exception();
}

void CJitter::Sra64(uint8 nAmount)
{
	throw std::exception();
}

void CJitter::Shl64()
{
	throw std::exception();
}

void CJitter::Shl64(uint8 nAmount)
{
	throw std::exception();
}

//Floating-Point
//------------------------------------------------
void CJitter::FP_PushCst(float)
{
	throw std::exception();
}

void CJitter::FP_PushSingle(size_t offset)
{
	m_Shadow.Push(MakeSymbol(SYM_FP_REL_SINGLE, static_cast<uint32>(offset)));
}

void CJitter::FP_PushWord(size_t)
{
	throw std::exception();
}

void CJitter::FP_PullSingle(size_t offset)
{
	STATEMENT statement;
	statement.op		= OP_MOV;
	statement.src1		= MakeSymbolRef(m_Shadow.Pull());
	statement.dst		= MakeSymbolRef(MakeSymbol(SYM_FP_REL_SINGLE, static_cast<uint32>(offset)));
	InsertStatement(statement);

	assert(GetSymbolSize(statement.src1) == GetSymbolSize(statement.dst));
}

void CJitter::FP_PullWordTruncate(size_t)
{
	throw std::exception();
}

void CJitter::FP_Add()
{
	SymbolPtr tempSym = MakeSymbol(SYM_FP_TMP_SINGLE, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_FP_ADD;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::FP_Mul()
{
	SymbolPtr tempSym = MakeSymbol(SYM_FP_TMP_SINGLE, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_FP_MUL;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::FP_Div()
{
	SymbolPtr tempSym = MakeSymbol(SYM_FP_TMP_SINGLE, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_FP_DIV;
	statement.src2	= MakeSymbolRef(m_Shadow.Pull());
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::FP_Sqrt()
{
	SymbolPtr tempSym = MakeSymbol(SYM_FP_TMP_SINGLE, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_FP_SQRT;
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::FP_Rcpl()
{
	SymbolPtr tempSym = MakeSymbol(SYM_FP_TMP_SINGLE, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_FP_RCPL;
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::FP_Neg()
{
	SymbolPtr tempSym = MakeSymbol(SYM_FP_TMP_SINGLE, m_nextTemporary++);

	STATEMENT statement;
	statement.op	= OP_FP_NEG;
	statement.src1	= MakeSymbolRef(m_Shadow.Pull());
	statement.dst	= MakeSymbolRef(tempSym);
	InsertStatement(statement);

	m_Shadow.Push(tempSym);
}

void CJitter::MD_PullRel(size_t)
{
	throw std::exception();
}

void CJitter::MD_PullRel(size_t, bool, bool, bool, bool)
{
	throw std::exception();
}

void CJitter::MD_PullRel(size_t, size_t, size_t, size_t)
{
	throw std::exception();
}

void CJitter::MD_PushRel(size_t)
{
	throw std::exception();
}

void CJitter::MD_PushRelExpand(size_t)
{
	throw std::exception();
}
