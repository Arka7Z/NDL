    /*
        BEGIN_LEGAL
        Intel Open Source License
        Copyright (c) 2002-2016 Intel Corporation. All rights reserved.
    */

    #include "pin.H"
    #include <iostream>
    #include <fstream>
    #include <bits/stdc++.h>
    using namespace std;

    #define traverse(container, it)  for(typeof(container.begin()) it = container.begin(); it != container.end(); it++)
    #define reverse_traverse(container, it)  for(typeof(container.rbegin()) it = container.rbegin(); it != container.rend(); it++)

    typedef struct {
        ADDRINT adr;
        string useType;
    } glove;
    typedef struct{
        string name;
        int registerFlag;                   //0 no comments(rbp,rsp),1 computation,2 accumulator,3-parameter passing,4 float 
        } reg;
    typedef struct {
    	string opcode;
    	string adr;
    	vector<string> operands;
        vector<glove> globals;
        vector<reg> registers;                               
    	string disassemble;
    	bool isValidJump;
    	}	instruction;

    typedef struct {
        int start_ind;
        int end_ind;
    	}	segment;

    typedef struct {
        int start_ind;
        int end_ind;
        int loopVarCount;
        }   ifSegment;

    typedef struct{
        unsigned int index;
        string useType;
        }   insUse;
    typedef struct{
        unsigned int index;
        string parent;
        int offsetIndex;
        }   arrrayLocal;
    typedef struct{
        string name;
        int forSegIndex;
        }   loopVar;

    typedef struct{
    	int size;
    	bool isArray;
    }	parameter; 

    map< string,list< map < string , pair< int, list< insUse > > > > > local_map;
    map< string, vector < arrrayLocal > > arrayLocal_map;
    map< string, vector < instruction > > M;
    map< string, vector < segment> > for_segment_map;
    map< string, vector < ifSegment > > if_segment_map;
    map< string, vector < loopVar > > loopVarMap;
    map< string, vector < segment > > nonForRegion;
    map< string, string > rtnAdr;
    map< string, int >  rtnCount;
    map< string, int >  insCount;
    map< string, vector < parameter > > paramMap;

    unsigned long long int convertToInt(string tmp);
    vector<string> parse(string opcode,string disassemble);
    long long int findCount(string adr);
    bool isRegister(string name);        


    KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool","o", "invocation.out", "specify output file name");

    ofstream OutFile;

    VOID Taken( const CONTEXT * ctxt)
    {
        ADDRINT TakenIP = (ADDRINT)PIN_GetContextReg( ctxt, REG_INST_PTR );
        OutFile << "Taken: IP = " << hex << TakenIP << dec << endl;
    }

    VOID Before(CONTEXT * ctxt)
    {
        ADDRINT BeforeIP = (ADDRINT)PIN_GetContextReg( ctxt, REG_INST_PTR);
        OutFile << "Before: IP = " << hex << BeforeIP << dec << endl;
    }


    VOID After(CONTEXT * ctxt)
    {
        ADDRINT AfterIP = (ADDRINT)PIN_GetContextReg( ctxt, REG_INST_PTR);
        OutFile << "After: IP = " << hex << AfterIP << dec << endl;
    }

    bool isRegister(string name)
    {
    	string registerNames[]={"rdx","rax","eax","ecx","rcx","edi","esi","rdi","rsi","r8","r9"};
    	bool flag=true;
    	for(unsigned int i=0;i<11;i++)
    		if (name==registerNames[i])
    			flag=true;
    	if(name.find("xm")!=string::npos)
    		flag=true;
    	if (name.find("rbp")!=string::npos)
    		flag=false;
    	return flag;
    }

    VOID RecordMemRead(VOID * ip, VOID * addr)
    {
        stringstream ss,sss;
        string instruction_adr=static_cast<std::ostringstream&>(ss<<setw(8)<<hex<<(reinterpret_cast<ADDRINT>(ip))).str();
        ADDRINT global_adr=/*static_cast<std::ostringstream&>(sss<<setw(8)<<hex<< (*/reinterpret_cast<ADDRINT>(addr)/*)).str()*/;
        string usageType="read";
        glove tmp;
        tmp.adr=global_adr;
        tmp.useType=usageType;
        traverse(M,map_it)
        {
            traverse(map_it->second,listIt)
            {
                if (convertToInt(listIt->adr)==convertToInt(instruction_adr))
                    listIt->globals.push_back(tmp);
            }
        }
        //cout<<" INStru "<< instruction_adr<<" global "<<global_adr<<endl;

    }
    VOID RecordMemWrite(VOID * ip, VOID * addr)
    {
        stringstream ss,sss;
        string instruction_adr=static_cast<std::ostringstream&>(ss<<setw(8)<<hex<<(reinterpret_cast<ADDRINT>(ip))).str();
        ADDRINT global_adr=/*static_cast<std::ostringstream&>(sss<<setw(8)<<hex<< (*/reinterpret_cast<ADDRINT>(addr)/*)).str()*/;
        string usageType="write";
        glove tmp;
        tmp.adr=global_adr;
        tmp.useType=usageType;
        traverse(M,map_it)
        {
            traverse(map_it->second,listIt)
            {
                if (convertToInt(listIt->adr)==convertToInt(instruction_adr))
                    listIt->globals.push_back(tmp);
            }
        }
    }

    VOID Instruction(INS ins, VOID *v)
    {
        stringstream ss;
        string adr=static_cast<std::ostringstream&>(ss<<setw(8)<<hex<< (INS_Address(ins))).str();
        if (insCount.find(adr)==insCount.end())
            insCount.insert(make_pair(adr,1));
        else
            insCount.find(adr)->second+=1;
        UINT32 memOperands = INS_MemoryOperandCount(ins);

        // Iterate over each memory operand of the instruction.
        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {
            if (INS_MemoryOperandIsRead(ins, memOp))
            {
                INS_InsertPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                    IARG_INST_PTR,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_END);
            }
 
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {
                INS_InsertPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                    IARG_INST_PTR,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_CONST_CONTEXT,
                    IARG_THREAD_ID,
                    IARG_END);
            }
        }
    }

    VOID Routine(RTN rtn,VOID *v)
    {
        stringstream ss;
        string adr=static_cast<std::ostringstream&>(ss<<setw(8)<<hex<< (RTN_Address(rtn))).str();
        if (adr.size()<=8)
        {
            if (rtnCount.find(adr)==rtnCount.end())
                rtnCount.insert(make_pair(adr,1));
            else
                rtnCount.find(adr)->second+=1;
        }
    }

    int findSizeOfOperand(string operand)
    {
        int size=0;

        if (operand.find("qword")!=string::npos)
			size=8;
		else if (operand.find("dword")!=string::npos)
			size=4;
		else if (operand.find("word")!=string::npos)
			size=2;
		else if (operand.find("byte")!=string::npos)
			size=1;
		else
			;
		return size;

    }
    bool isStopWord( string rtn_name)
    {
    	string stop[]={"printf@plt","puts@plt","__gmon_start__@plt","_fini","_libc_csu_init","frame_dummy","__do_global_dtors_aux","register_tm_clones","deregister_tm_clones","_start","__libc_start_main@plt",".plt","_init",".plt.got","__libc_csu_fini","__libc_csu_init","__stack_chk_fail@plt"};
    	for(int i=0;i<=16;i++)
    		if (rtn_name==stop[i])
    			return true;
    	return false;

    }

    vector<string> parse(string opcode,string disassemble)
    {
    	istringstream iss(disassemble);
    	vector<string> tokens;
    	copy(istream_iterator<string>(iss), istream_iterator<string>(),back_inserter(tokens));
    	vector<string> operands;
    	if (tokens.size()==1)
    	{
    		//do nothing 0 operands case
    	}
    	else if(tokens.size()==2)
    	{
    		//unary operand and no comma
    		operands.push_back(tokens[1]);
    	}
    	else{
    		bool found_comma=false;
    		string tmp1="";
    		string tmp2="";
    		for(unsigned int i=1;i<tokens.size();i++)
    			{
    				if (!found_comma)
    					{
    						if (tokens[i].find(",")==string::npos)
    								tmp1.size()?tmp1+=" "+tokens[i]:tmp1+=tokens[i];
    						else
    							{
    								tokens[i].resize(tokens[i].size()-1);
    								tmp1.size()?tmp1+=" "+tokens[i]:tmp1+=tokens[i];
    								found_comma=true;
    							}
    					}
    				else
    					{
    						tmp2.size()?tmp2+=" "+tokens[i]:tmp2+=tokens[i];
    					}
    			}

    		operands.push_back(tmp1);
    		operands.push_back(tmp2);

    	}
    	return operands;

    }

    unsigned long long int convertToInt(string tmp)
    {
        unsigned long long int x;
        std::stringstream ss;
        ss << std::hex <<tmp;
        ss >> x;
        return x;
    }

    bool occurs_previously(string adr,vector<string> ins_adr)
    {
        adr=adr.substr(2,string::npos);
        cout<<" ADR "<<adr<<endl;
        traverse(ins_adr,it){
            string tmp=string(*it);
            cout<<tmp<<endl;
            if (convertToInt(tmp)==convertToInt(adr))
                return true;
        }
        return false;
    }

    bool occurs_previously(string adr,vector<instruction> ins)
    {
        adr=adr.substr(2,string::npos);
        traverse(ins,it){
        string tmp=string(it->adr);

        if (convertToInt(tmp)==convertToInt(adr))
            return true;
        }
        return false;
    }


    int countNumberOfLoops(string rtn_name)
    {
        int count=0;
        vector<string> ins_adr;
        map<string,vector <instruction> > :: iterator map_it=M.find(rtn_name);
        {
            string rtn_name=map_it->first;
            for (vector<instruction> :: iterator list_it=map_it->second.begin();list_it!=map_it->second.end();list_it++)
                {
                    if (list_it->opcode=="jmp" || list_it->opcode=="jnle"||list_it->opcode=="jle" || list_it->opcode=="jnl" || list_it->opcode=="jnz" || list_it->opcode=="jl"){
                        if (occurs_previously(list_it->operands[0],ins_adr)){
                            count++;
                            list_it->isValidJump=true;
                        }
                        else
                            list_it->isValidJump=false;

                        }
                    else{
                        ins_adr.push_back(list_it->adr);
                        list_it->isValidJump=false;
                    }
                }

        }
        cout<<"RTN NAME "<<rtn_name<<" count "<<count<<endl;
        return count;

    }

    int findStartIndex(string tmp,vector< instruction > ins)
    {
        int ind=0;
        traverse(ins,it){
            if(convertToInt(it->adr)==convertToInt(tmp))
                {
                    return ind;
                }
            ind++;
        }
        return ind;
    }

    int findEndIndex(string tmp,vector< instruction > ins)
    {
        int ind=0;
        traverse(ins,it){
            if(convertToInt(it->adr)==convertToInt(tmp))
                {
                    return ind;
                }
            ind++;
        }
        return ind;
    }

    void segmentizeFor()
    {
        traverse(M,map_it)
        {
            string rtn_name=map_it->first;
            int num_for_loops=countNumberOfLoops(rtn_name);
            vector<segment> segtmp;

            if (num_for_loops>0)
            {
                for (vector<instruction> :: reverse_iterator list_it=map_it->second.rbegin();list_it!=map_it->second.rend();list_it++)
                    {
                        if ((list_it->opcode=="jmp" ||list_it->opcode=="jnle"||list_it->opcode=="jle" || list_it->opcode=="jnl" || list_it->opcode=="jnz" || list_it->opcode=="jl")&& list_it->isValidJump==true)
                            {
                                int startInd=findStartIndex(list_it->operands[0],map_it->second);
                                int endInd=findEndIndex(list_it->adr,map_it->second);
                                segment tmp;
                                tmp.start_ind=startInd;
                                tmp.end_ind=endInd;
                                segtmp.push_back(tmp);
                            }
                    }
                for_segment_map.insert(make_pair(rtn_name,segtmp));

            }
        }

    }

    int findIndex(string rtn_name,string adr)
    {
        int ind=0;
        typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
        traverse(map_it->second,it)
        {
            if (convertToInt(adr)==convertToInt(it->adr))
                return ind;
            ++ind;
        }
        return -1;
    }

    bool hasjmp(vector<instruction> ins,string rtn_name,int upper_limit)
    {
        traverse(ins,it)
        {
            if (it->opcode=="jmp"&&findIndex(rtn_name,it->adr)<upper_limit)
                return true;
        }
        return false;
    }

    void segmentizeIf()
    {
        vector<instruction> ins;
        vector<ifSegment> seg_vec;

        traverse(M,map_it)
        {
            int startInd;
            int endInd;
            string rtnName=map_it->first;
            reverse_traverse(map_it->second,list_it)
            {
                if (list_it->opcode=="jnle"||list_it->opcode=="jle" || list_it->opcode=="jnl" || list_it->opcode=="jnz")
                    {
                        if (occurs_previously(list_it->operands[0],ins))
                            {
                                ifSegment tmp;
                                if (hasjmp(ins,rtnName,findIndex(rtnName,list_it->operands[0])))
                                    {
                                       startInd=findIndex(rtnName,list_it->adr);
                                       endInd=findIndex(rtnName,list_it->operands[0]);
                                    }
                                else
                                    {
                                       startInd=findIndex(rtnName,list_it->adr);
                                       endInd=findIndex(rtnName,list_it->operands[0])-1;
                                    }
                                tmp.start_ind=startInd-1;
                                tmp.end_ind=endInd;
                                tmp.loopVarCount=0;
                                seg_vec.push_back(tmp);

                            }
                        else
                            ;

                    }
                else
                    {
                        ins.push_back(*list_it);
					}
            }
            if_segment_map.insert(make_pair(rtnName,seg_vec));
            seg_vec.clear();
            ins.clear();

        }

    }

    void rectify()
    {
        traverse(for_segment_map,for_it)
        {
            if (for_it->second.size())
            {
                traverse(for_it->second,it1)
                {
                    int forStartIndex=it1->start_ind;
                    typeof(if_segment_map.begin()) if_it=if_segment_map.find(for_it->first);
                    traverse(if_it->second,it)
                    {
                        if (it->start_ind==forStartIndex+1)
                            if_it->second.erase(it);
                    }
                }
            }
        }
    }

    vector<string> getUniqueLocals(string rtn_name)
    {
        set< string > locals;
        typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
        traverse(map_it->second,list_it)
        {
            traverse(list_it->operands,it)
            {
                if ((*it).find("[")!=string::npos)
                {
                    int start=(*it).find("[");
                    int end=(*it).find("]");
                    string tmp=(*it).substr(start+1,end-start-1);
                    locals.insert(tmp);
                }
            }
        }
        vector<string> vec;
        std::copy(locals.begin(), locals.end(), std::back_inserter(vec));
        return vec;
    }

    void buildParamMap()
    {
    	traverse(M,map_it)
    	{
    		string rtnName=map_it->first;
    		string called;
    		vector<instruction> insList;
    		vector<instruction> regInsList;
    		map<string,int> regMap;                                          //int -1 is main source -2 is inb2wintransit ,nonzero is size
    		set<string> isTransit;
    		set<string> isArray;
    		map<string,string> hasTransit;
    		bool hasCall=false;    
    		traverse(map_it->second,listIt)
    		{
    			if(listIt->opcode=="call" || listIt->opcode=="call_near" )
    			{
    				called=(listIt->operands[0]).substr(2,string::npos);
    				hasCall=true;
    				break;
    			}
    			else
    				insList.push_back(*listIt);
    		}
    		reverse_traverse(insList,insListIt)
    		{
    			if ((insListIt->opcode).find("mov")!=string::npos )
    			{
    				if(isRegister(insListIt->operands[0]))
    					regInsList.push_back(*insListIt);
    				else
    					;
    			}
    			else if ((insListIt->opcode).find("lea")!=string::npos)
    			{
					if(isRegister(insListIt->operands[0]))
    				{
    					regInsList.push_back(*insListIt);
   					}
    				isArray.insert(insListIt->operands[0]); 
    				cout<<"INserting in is arraySet "<< (insListIt->operands[0])<<endl;  			
				}
    			else
    				break;
    		}
    		std::reverse(regInsList.begin(),regInsList.end());
    		
    		traverse(regInsList,it)
    		{
    			string op1=it->operands[0];
    			string op2=it->operands[1];
    			//int regSize;
    			if (!isRegister(op2))
    				{
    					//regSize=findSizeOfOperand(op2);
    					//cout<<" REG REG "<<regSize<<endl;
    					regMap.insert(make_pair(op1,findSizeOfOperand(op2)));
					}
    			else
    				{
    					//regSize=findSizeOfOperand(op2);
    					//cout<<" REG REG "<<regSize<<endl;
    					regMap.insert(make_pair(op1,(regMap.find(op2))->second));
    					isTransit.insert(op2);
    					hasTransit.insert(make_pair(op1,op2));
    				}
    		}
    		vector<parameter> paramvec;
    		traverse(regMap,it)
    		{
    			string regName=it->first;
    			parameter tmpPar;
    			if (isTransit.find(regName)==isTransit.end())
    			{
    				if (isArray.find(regName)==isArray.end())
    				{
    					if(hasTransit.find(regName)==hasTransit.end())
    					{
	    					cout<<"NOT ARRAY "<<regName<<endl;
	    					tmpPar.size=it->second;
	    					tmpPar.isArray=false;
    					}
    					else
    					{
    						if (isArray.find(hasTransit.find(regName)->second)!=isArray.end())
    						{
    							tmpPar.size=-1;
    							tmpPar.isArray=true;
    						}
    						else
    						{
		    					tmpPar.size=it->second;
		    					tmpPar.isArray=false;
	    					}
    					}
    				}
    				else
    				{
    					tmpPar.size=-1;
    					tmpPar.isArray=true;
    				}
    				paramvec.push_back(tmpPar);
    			}
    		}
    		if(hasCall)
    			paramMap.insert(make_pair(called,paramvec));

    	}
    }
    void buildLocalMap()
    {
        vector<string> locals;
        map< string, pair< int, list<insUse> > > tmpMap;
        map< string, int> localSize;
        traverse(M,map_it)
        {
            list< map< string , pair< int, list< insUse > > > > listOfLocals;
            string rtn_name=map_it->first;
            locals=getUniqueLocals(rtn_name);
            map<string,bool> flag;
            traverse(locals,it)
            flag.insert(make_pair(*it,false));
            traverse(locals,it)
            {
                string name=*it;
                int size=0;
                traverse(map_it->second,list_it)
                {
                    traverse(list_it->operands,op_it)
                    {
                        if ((*op_it).find(*it)!=string::npos)
                        {
                            if ((*op_it).find("qword")!=string::npos)
                                size=8;
                            else if ((*op_it).find("dword")!=string::npos)
                                size=4;
                            else if ((*op_it).find("word")!=string::npos)
                                size=2;
                            else if ((*op_it).find("byte")!=string::npos)
                                size=1;

                        }
                    }
                    
                }
                localSize.insert(make_pair(name,size));
            }

            traverse(locals,it)
            {
                string name=*it;
                int index=0;
                pair< int, list<insUse> > tmpPair;
                tmpPair.first=localSize[(name)];
                traverse(map_it->second,list_it)                                     //listIt iterates over the list of instructions corresponding to a rtn
                {
                    int operandIndex=0;
                    insUse tmp;
                    traverse(list_it->operands,op_it)
                    {
                        if ((*op_it).find(*it)!=string::npos)
                        {
                            tmp.index=index;
                            tmp.useType=operandIndex?"read":"write";
                            tmpPair.second.push_back(tmp);
                        }
                        operandIndex++;
                    }
                    
                    index++;
                }
                tmpMap.insert(make_pair(name,tmpPair));
            }
            listOfLocals.push_back(tmpMap);
            local_map.insert(make_pair(rtn_name,listOfLocals));  
        }
    }

    void buildArrayLocalMap(string rtn_name)
    {
        string current_parent;
        vector< arrrayLocal > locals;
        int offsetIndex=1;
        typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
        int index=0;
        traverse(map_it->second,list_it)
        {
            if(list_it->opcode=="mov"&&list_it->operands[0]=="rax")
            {
                int start=(list_it->operands[1]).find("[");
                int end=(list_it->operands[1]).find("]");
                current_parent=(list_it->operands[1]).substr(start+1,end-start-1);
                offsetIndex=1;
                index++;
                continue;
            }

            traverse(list_it->operands,it)
            {

                if ((*it).find("rax+")!=string::npos)
                {   
                    arrrayLocal arrrayLocaltmp;
                    int start=(*it).find("[");
                    int end=(*it).find("]");
                    string tmp=(*it).substr(start+1,end-start-1);
                    arrrayLocaltmp.index=index;
                    arrrayLocaltmp.parent=current_parent;
                    arrrayLocaltmp.offsetIndex=offsetIndex;
                    locals.push_back(arrrayLocaltmp);
                    offsetIndex++;
                }
            }
            index++;
        }
       
        arrayLocal_map.insert(make_pair(rtn_name,locals));   
    }

    /*
     * Instrumentation routines
     */
    VOID ImageLoad(IMG img, VOID *v)
    {

     if(IMG_IsMainExecutable(img))							                                //on the main image only
        for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))               //loops on the sections in the image
        {

            for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))           //loops on the routines called in each section
            {

                RTN_Open( rtn );
                string routine_name=(RTN_Name(rtn));

                if (isStopWord(routine_name))
                    {
    				    RTN_Close(rtn);
                        continue;
                    }
                stringstream ss;
                string rtn_adr=static_cast<std::ostringstream&>(ss<<setw(8)<<hex<< (RTN_Address(rtn))).str();
                rtnAdr.insert(make_pair(routine_name,rtn_adr));
                for( INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) )      //loops on the instructions in the routine
                {
    				instruction ist;
    				string tmp;
    				string str=INS_Mnemonic(ins);
    				tmp.resize(str.size());
    				std::transform(str.begin(),str.end(),tmp.begin(),::tolower);
    				ist.opcode=tmp;
                    stringstream ss;
                    string str_tmp=static_cast<std::ostringstream&>(ss<<setw(8)<<hex<< (INS_Address(ins))).str();
    				ist.adr= str_tmp ;
    				ist.disassemble=INS_Disassemble(ins);
    				ist.operands=parse(tmp,ist.disassemble);
                    reg regtmp;
                    traverse(ist.operands,op_it)
                    {
                        if ((*op_it).find("eax")!=string::npos)
                        {
                            regtmp.name="eax";
                            regtmp.registerFlag=1;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("rax")!=string::npos)
                        {
                            regtmp.name="rax";
                            regtmp.registerFlag=1;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("ecx")!=string::npos)
                        {
                            regtmp.name="ecx";
                            regtmp.registerFlag=2;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("rcx")!=string::npos)
                        {
                            regtmp.name="rcx";
                            regtmp.registerFlag=2;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("edi")!=string::npos)
                        {
                            regtmp.name="edi";
                            regtmp.registerFlag=3;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("esi")!=string::npos)
                        {
                            regtmp.name="esi";
                            regtmp.registerFlag=3;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("rdi")!=string::npos)
                        {
                            regtmp.name="rdi";
                            regtmp.registerFlag=3;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("rsi")!=string::npos)
                        {
                            regtmp.name="rsi";
                            regtmp.registerFlag=3;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("r8")!=string::npos)
                        {
                            regtmp.name="r8";
                            regtmp.registerFlag=3;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("r9")!=string::npos)
                        {
                            regtmp.name="r9";
                            regtmp.registerFlag=3;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("rbp")!=string::npos)
                        {
                            regtmp.name="rbp";
                            regtmp.registerFlag=0;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("rsp")!=string::npos)
                        {
                            regtmp.name="rsp";
                            regtmp.registerFlag=0;
                            ist.registers.push_back(regtmp);
                        }
                        else if ((*op_it).find("xm")!=string::npos)
                        {
                            regtmp.name="xm";
                            regtmp.registerFlag=4;
                            ist.registers.push_back(regtmp);
                        }
                        else
                            ;                                                                                                
                    }

    			    if (M.find(routine_name)==M.end())
                        {
                            vector < instruction > lis;
    						lis.push_back(ist);
    						M.insert(make_pair(routine_name,lis));
                        }
    				else
    					{
    						M[routine_name].push_back(ist);
    					}

                }

                RTN_Close( rtn );
            }
        }
    }
    void buildNonForRegions()
    {
        traverse(for_segment_map,for_it)
        {
            string rtn_name=for_it->first;
            vector<segment> segments;
            typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
            vector<bool> isFor(map_it->second.size(),false);
            traverse(for_it->second,forSegmentIt)
            {
                unsigned int startInd=forSegmentIt->start_ind;
                unsigned int endInd=forSegmentIt->end_ind;
                for(unsigned int i= startInd;i<=endInd;i++)
                {
                    if (isFor[i]==false)
                    {    
                        isFor[i]=true;
                    }

                }
            }
            unsigned int startInd=0;
            bool forIsRunning=false;
            for(unsigned int i=0;i<isFor.size();i++)
            {
                if(!isFor[i])
                {    
                    if(forIsRunning)
                    {
                        startInd=i;
                        forIsRunning=false;
                    }
                }
                else
                {
                    if(forIsRunning==false)
                    {
                        forIsRunning=true;
                        segment tmp;
                        tmp.start_ind=startInd;
                        tmp.end_ind=i-1;
                        segments.push_back(tmp);
                    }
                }
            }
            nonForRegion.insert(make_pair(rtn_name,segments));
        }
        traverse(M,map_it)
        {
            if(for_segment_map.find(map_it->first)==for_segment_map.end())
            {
                segment tmp;
                tmp.start_ind=0;
                tmp.end_ind=map_it->second.size()-1;
                vector<segment> segments;
                segments.push_back(tmp);
                nonForRegion.insert(make_pair(map_it->first,segments));
            }
        }
    }

    void printRegionByRegion()
    {
    	traverse(M,map_it)
    	{
    		cout<<"ROUTINE NAME: "<<map_it->first<<endl;
    		string rtn_name=map_it->first;
    		if (nonForRegion.find(map_it->first)!=nonForRegion.end())
    		{
	    		typeof(nonForRegion.find(map_it->first)) nfor_it=nonForRegion.find(map_it->first);
	    		{
	    			string rtn_name=nfor_it->first;
		            //cout<<"Routine Name: "<<rtn_name<<endl;
		            typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
		            traverse(nfor_it->second,forSegIt)
		            {
		                unsigned int startInd=forSegIt->start_ind;
		                unsigned int endInd=forSegIt->end_ind;
		                int write=0;
		                int read=0;
		                int numberOfLocals=0;
		                set<string> locals;
		                cout<<"NON FOR SEGMENT Start Index: "<<startInd<<". End Index: "<<endInd<<"."<<endl;
		                for (unsigned int i=startInd;i<=endInd;i++)
	                	{
						traverse(lm_it->second,lis_it)
		                    {
		                        
		                        traverse((*lis_it),it)
		                        {   
		                            traverse(it->second.second,it1)
		                            {   
		                                if(it1->index==i)
		                                {
		                                    numberOfLocals++;
		                                    locals.insert(it->first);
		                                    it1->useType=="write"?(write++):(read++);
		                                }
		                            }
		                        }
		                    }
	                	}
	                	cout<<"Number of Local Operaration: "<<numberOfLocals<<". Number of distinct Locals: "<<locals.size()<<". Read: "<<read<<". Write: "<<write<<endl;
						//cout<<"===================================================================================="<<endl;
		    		}

	    		}
	    	}

	    	if (for_segment_map.find(map_it->first)!=for_segment_map.end())
	    	{
	    		typeof(for_segment_map.find(map_it->first)) for_it=for_segment_map.find(map_it->first);
	    		{
		            string rtn_name=for_it->first;
		            typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
		            typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
		            traverse(for_it->second,forSegIt)
		            {
		                unsigned int startInd=forSegIt->start_ind;
		                unsigned int endInd=forSegIt->end_ind;
		                int write=0;
		                int read=0;
		                int numberOfLocals=0;
		                set<string> locals;
		                cout<<"FOR LOOP SEGMENT Start Index: "<<startInd<<". End Index: "<<endInd<<"."<<endl;
		                for (unsigned int i=startInd;i<=endInd;i++)
	                	{
							traverse(lm_it->second,lis_it)
			                    {
			                        
			                        traverse((*lis_it),it)
			                        {   
			                            traverse(it->second.second,it1)
			                            {   
			                                if(it1->index==i)
			                                {
			                                    numberOfLocals++;
			                                    locals.insert(it->first);
			                                    it1->useType=="write"?(write++):(read++);
			                                }
			                            }
			                        }
			                    }
	                	}
		                cout<<"Number of Local Operaration: "<<numberOfLocals<<". Number of distinct Locals: "<<locals.size()<<". Read: "<<read<<". Write: "<<write<<endl;
						//cout<<"===================================================================================="<<endl;
			    		list<arrrayLocal> listOfArrayLocal;
			    		if (arrayLocal_map.find(rtn_name)!=arrayLocal_map.end())
			            {
			            	typeof(arrayLocal_map.find(rtn_name)) aL_it=arrayLocal_map.find(rtn_name);
			                {
			                    traverse(aL_it->second,list_it)
			                    {
			                        if (list_it->index>=startInd && list_it->index<=endInd)
			                            listOfArrayLocal.push_back(*list_it);
			                    }
			                    cout<<"NUMBER OF ARRAY LOCALS USED: "<<listOfArrayLocal.size()<<endl;
			                }
		                }
		    		}
				}
	    	}
	    	if (if_segment_map.find(map_it->first)!=if_segment_map.end())
	    	{
	    		typeof(if_segment_map.find(map_it->first)) for_it=if_segment_map.find(map_it->first);
	    		{
		           string rtn_name=for_it->first;
		            //cout<<"Routine Name: "<<rtn_name<<endl;
		            typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
		            typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
		            traverse(for_it->second,forSegIt)
		            {
		                unsigned int startInd=forSegIt->start_ind;
		                unsigned int endInd=forSegIt->end_ind;
		                int write=0;
		                int read=0;
		                int numberOfLocals=0;
		                set<string> locals;
		                cout<<"IF SEGMENT Start Index: "<<startInd<<". End Index: "<<endInd<<". Number of loop variables used in comparision: "<<forSegIt->loopVarCount<<endl;
		                for (unsigned int i=startInd;i<=endInd;i++)
	                	{
						traverse(lm_it->second,lis_it)
		                    {
		                        
		                        traverse((*lis_it),it)
		                        {   
		                            traverse(it->second.second,it1)
		                            {   
		                                if(it1->index==i)
		                                {
		                                    numberOfLocals++;
		                                    locals.insert(it->first);
		                                    it1->useType=="write"?(write++):(read++);
		                                }
		                            }
		                        }
		                    }
	                	}
		                cout<<"Number of Local Operaration: "<<numberOfLocals<<". Number of distinct Locals: "<<locals.size()<<". Read: "<<read<<". Write: "<<write<<endl;
						list<arrrayLocal> listOfArrayLocal;
			    		if (arrayLocal_map.find(rtn_name)!=arrayLocal_map.end())
			            {
			            	typeof(arrayLocal_map.find(rtn_name)) aL_it=arrayLocal_map.find(rtn_name);
			                {
			                    traverse(aL_it->second,list_it)
			                    {
			                        if (list_it->index>=startInd && list_it->index<=endInd)
			                            listOfArrayLocal.push_back(*list_it);
			                    }
			                    cout<<"NUMBER OF ARRAY LOCALS USED: "<<listOfArrayLocal.size()<<endl;
			                }
		                }	
					//cout<<"===================================================================================="<<endl;
		    		}
	                    			
	    		}
	    	}
	    	cout<<"========================================================================================="<<endl;
    	}
    	cout<<"::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::"<<endl<<endl;	
    }

    void printNonForRegions()
    {
        cout<<"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl<<"PRINTING THE NON FOR REGIONS"<<endl<<"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl;
        traverse(nonForRegion,for_it)
        {
            string rtn_name=for_it->first;
            cout<<"Routine Name: "<<rtn_name<<endl;
            typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
            typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
            traverse(for_it->second,forSegIt)
            {
                unsigned int startInd=forSegIt->start_ind;
                unsigned int endInd=forSegIt->end_ind;
                int write=0;
                int read=0;
                int numberOfLocals=0;
                set<string> locals;
                cout<<"NON FOR SEGMENT Start Index: "<<startInd<<". End Index: "<<endInd<<"."<<endl;
                /*list< pair< int,string> > localsForThisSeg;
                list< pair<int,int> > listOfIf;
                list<arrrayLocal> listOfArrayLocal;
                string local_name;
                int localSize;
                typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
                {
                    traverse(lm_it->second,lis_it)
                    {
                        traverse((*lis_it),it)
                        {   
                            local_name=it->first;
                            localSize=it->second.first;
                            traverse(it->second.second,it1)
                            {
                                if (it1->index>=startInd && it1->index<=endInd)   
                                    localsForThisSeg.push_back(make_pair(it1->index,it1->useType));
                            }
                            if (localsForThisSeg.size())
                            {
                                cout<<"Local Name: "<<local_name<<". Size: "<<localSize<<". Usage Index: "<<endl;
                                traverse(localsForThisSeg,localsForThisSegIt)
                                {
                                    cout<<"Index: "<<localsForThisSegIt->first<<". Use Type: "<<localsForThisSegIt->second<<endl;
                                }
                            }
                            localsForThisSeg.clear();
                        }
                      
                        cout<<"--------------------------------------------------------------------------"<<endl;
                    }
                    
                }


                typeof(if_segment_map.find(rtn_name)) if_it=if_segment_map.find(rtn_name);
                {
                    traverse(if_it->second,list_it)
                    {   
                        if(list_it->start_ind>=startInd && list_it->end_ind<=endInd)
                        {
                            listOfIf.push_back(make_pair(list_it->start_ind,list_it->end_ind));
                        }

                        //listOfIf.clear();
                    }
                     if(listOfIf.size())
                     {
                        cout<<"Number of if loops: "<<listOfIf.size()<<endl;
                        int tmpIndex=0;
                        traverse(listOfIf,listOfIfIt)
                        {
                            cout<<"If block Number: "<<(tmpIndex+1)<<". Start Index: "<<listOfIfIt->first<<". End Index: "<<listOfIfIt->second<<endl;
                        }
                        cout<<"----------------------------------------------------------------------------------"<<endl;
                    }
             
                }
             
                typeof(arrayLocal_map.find(rtn_name)) aL_it=arrayLocal_map.find(rtn_name);
                {
                    traverse(aL_it->second,list_it)
                    {
                        if (list_it->index>=startInd && list_it->index<=endInd)
                            listOfArrayLocal.push_back(*list_it);
                    }
                    if(listOfArrayLocal.size())
                    {
                        cout<<" Array locals used in this segment are as follows: "<<endl;
                        traverse(listOfArrayLocal,listOfArrayLocalIt)
                        {
                            cout<<"Index: "<<listOfArrayLocalIt->index<<". Parent: "<<listOfArrayLocalIt->parent<<". offsetIndex: "<<listOfArrayLocalIt->offsetIndex<<endl;
                        }
                    }
                }*/

                for (unsigned int i=startInd;i<=endInd;i++)
                //for(vector<instruction> :: iterator list_it=map_it->second.begin();list_it!=map_it->second.end();list_it++)
                {
                    //cout<<" INDEX: "<<i<<endl;
                    /*cout<<map_it->second[i].opcode<<" "<<map_it->second[i].adr;
                    cout<<". Number of operands: "<<map_it->second[i].operands.size()<<". ";
                    for(unsigned int j=0;j<map_it->second[i].operands.size();j++)
                        cout<<"Operand "<<(j+1)<<" "<<map_it->second[i].operands[j]<<".";
                    cout<<endl;*/
                    /*cout<<"GLOBALS: "<<endl;
                    traverse(map_it->second[i].globals,globIt)
                        cout<<(globIt)->adr<<" "<<globIt->useType<<"  ";*/
                    /*cout<<"REGISTERS: ";
                    traverse(map_it->second[i].registers,regIt)
                        cout<<regIt->name<<" "<<regIt->registerFlag<<endl;
                    cout<<endl;*/
                    traverse(lm_it->second,lis_it)
                    {
                        
                        traverse((*lis_it),it)
                        {   
                            traverse(it->second.second,it1)
                            {   
                                if(it1->index==i)
                                {
                                    //cout<<" helo size helo size "<<lm_it->second.size()<<endl;
                                    //cout<<"Local Name: "<<it->first<<" Size: "<<it->second.first<<endl;
                                    numberOfLocals++;
                                    locals.insert(it->first);
                                    //cout<<"Usage: "<<endl;
                                    //cout<<"Index: "<<it1->index<<"Type: "<<it1->useType<<endl;
                                    it1->useType=="write"?(write++):(read++);
                                    //cout<<endl;
                                }
                            }
                        }
                    }
                }
                cout<<"Number of Local Operaration: "<<numberOfLocals<<". Number of distinct Locals: "<<locals.size()<<". Read: "<<read<<". Write: "<<write<<endl;

                cout<<"===================================================================================="<<endl;
            }   

        }
    }

    void printForRegions()
    {
        cout<<"PRINTING THE FOR REGIONS"<<endl<<"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ "<<endl;
        traverse(for_segment_map,for_it)
        {
            string rtn_name=for_it->first;
            cout<<"Routine Name: "<<rtn_name<<endl;
            typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
            typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
            traverse(for_it->second,forSegIt)
            {
                unsigned int startInd=forSegIt->start_ind;
                unsigned int endInd=forSegIt->end_ind;
                int write=0;
                int read=0;
                int numberOfLocals=0;
                set<string> locals;
                cout<<"FOR LOOP SEGMENT Start Index: "<<startInd<<". End Index: "<<endInd<<"."<<endl;
                /*list< pair< int,string> > localsForThisSeg;
                list< pair<int,int> > listOfIf;
                list<arrrayLocal> listOfArrayLocal;
                string local_name;
                int localSize;
                typeof(local_map.find(rtn_name)) lm_it=local_map.find(rtn_name);
                {
                    traverse(lm_it->second,lis_it)
                    {
                        cout<<lm_it->second.size()<<" SIze size size"<<endl;
                        traverse((*lis_it),it)
                        {   
                            local_name=it->first;
                            localSize=it->second.first;
                            traverse(it->second.second,it1)
                            {
                                if (it1->index>=startInd && it1->index<=endInd)   
                                    localsForThisSeg.push_back(make_pair(it1->index,it1->useType));
                            }
                        if (localsForThisSeg.size())
                        {
                            cout<<"Local Name: "<<local_name<<". Size: "<<localSize<<". Usage Index: "<<endl;
                            traverse(localsForThisSeg,localsForThisSegIt)
                            {
                                cout<<"Index: "<<localsForThisSegIt->first<<". Use Type: "<<localsForThisSegIt->second<<endl;
                            }
                        }
                        localsForThisSeg.clear();
                        cout<<"--------------------------------------------------------------------------"<<endl;
                        }

                    }
                    
                }


                typeof(if_segment_map.find(rtn_name)) if_it=if_segment_map.find(rtn_name);
                {
                    traverse(if_it->second,list_it)
                    {   
                        if(list_it->start_ind>=startInd && list_it->end_ind<=endInd)
                        {
                            listOfIf.push_back(make_pair(list_it->start_ind,list_it->end_ind));
                        }
                    }
                }
                if(listOfIf.size())
                {
                    cout<<"Number of if loops: "<<listOfIf.size()<<endl;
                    int tmpIndex=0;
                    traverse(listOfIf,listOfIfIt)
                    {
                        cout<<"If block Number: "<<(tmpIndex+1)<<". Start Index: "<<listOfIfIt->first<<". End Index: "<<listOfIfIt->second<<endl;
                    }
                    cout<<"----------------------------------------------------------------------------------"<<endl;
                }
                

                typeof(arrayLocal_map.find(rtn_name)) aL_it=arrayLocal_map.find(rtn_name);
                {
                    traverse(aL_it->second,list_it)
                    {
                        if (list_it->index>=startInd && list_it->index<=endInd)
                            listOfArrayLocal.push_back(*list_it);
                    }
                }
                if(listOfArrayLocal.size())
                {
                    cout<<" Array locals used in this segment are as follows: "<<endl;
                    traverse(listOfArrayLocal,listOfArrayLocalIt)
                    {
                        cout<<"Index: "<<listOfArrayLocalIt->index<<". Parent: "<<listOfArrayLocalIt->parent<<". offsetIndex: "<<listOfArrayLocalIt->offsetIndex<<endl;
                    }
                }*/


                for (unsigned int i=startInd;i<=endInd;i++)
                //for(vector<instruction> :: iterator list_it=map_it->second.begin();list_it!=map_it->second.end();list_it++)
                {
                    cout<<" INDEX: "<<i<<endl;
                    cout<<map_it->second[i].opcode<<" "<<map_it->second[i].adr;
                    /*cout<<". Number of operands: "<<map_it->second[i].operands.size()<<". ";
                    for(unsigned int j=0;j<map_it->second[i].operands.size();j++)
                        cout<<"Operand "<<(j+1)<<" "<<map_it->second[i].operands[j]<<".";
                    cout<<endl;*/
                    /*cout<<"GLOBALS: "<<endl;
                    traverse(map_it->second[i].globals,globIt)
                        cout<<(globIt)->adr<<" "<<globIt->useType<<"  ";*/
                    /*cout<<"REGISTERS: ";
                    traverse(map_it->second[i].registers,regIt)
                        cout<<regIt->name<<" "<<regIt->registerFlag<<endl;
                    cout<<endl;*/
                    traverse(lm_it->second,lis_it)
                    {
                        
                        traverse((*lis_it),it)
                        {   
                            traverse(it->second.second,it1)
                            {   
                                if(it1->index==i)
                                {
                                    //cout<<" helo size helo size "<<lm_it->second.size()<<endl;
                                    //cout<<"Local Name: "<<it->first<<" Size: "<<it->second.first<<endl;
                                    numberOfLocals++;
                                    locals.insert(it->first);
                                    //cout<<"Usage: "<<endl;
                                    //cout<<"Index: "<<it1->index<<"Type: "<<it1->useType<<endl;
                                    it1->useType=="write"?(write++):(read++);
                                    //cout<<endl;
                                }
                            }
                        }
                    }
                }
                cout<<"Number of Local Operaration: "<<numberOfLocals<<". Number of distinct Locals: "<<locals.size()<<". Read: "<<read<<". Write: "<<write<<endl;
                cout<<"===================================================================================="<<endl;
            }   

        }

    }

    void printRegions()
    {
        printForRegions();
        printNonForRegions();
    }

    void buildIfVarMap()
    {
        traverse(loopVarMap,loopVarMapIt)
        {
            
            string rtnName=loopVarMapIt->first;
            typeof(for_segment_map.find(rtnName)) forIt=for_segment_map.find(rtnName);
            typeof(if_segment_map.find(rtnName)) ifIt=if_segment_map.find(rtnName);
            typeof(M.find(rtnName)) map_it=M.find(rtnName);
            traverse(loopVarMapIt->second,listIt)
            {
                string name=listIt->name;
                int forInd=listIt->forSegIndex;
                int startInd=forIt->second[forInd].start_ind;
                int endInd=forIt->second[forInd].end_ind;
                traverse(ifIt->second,ifListIt)
                {
                    //cout<<" For "<<startInd<<" "<<endInd<<".IF "<<ifListIt->start_ind<<" "<<ifListIt->end_ind<<endl;
                    if (ifListIt->start_ind>=startInd && ifListIt->start_ind<=endInd)
                    {
                        int ifStart=ifListIt->start_ind;
                        instruction instmp=map_it->second[ifStart];
                        traverse(instmp.operands,op_it)
                        {
                            cout<<" NAME "<<name<<" op it "<<(*op_it)<<endl;
                            if ((*op_it)==name || ((*op_it)=="eax"))
                                ifListIt->loopVarCount+=1;
                        }
                    }
                }
            }
        }
    }
    void buildLoopVarMap()
    {
        traverse(for_segment_map,for_it)
        {
            string rtn_name=for_it->first;
            int forInd=0;
            traverse(for_it->second,forSegIt)
            {
                unsigned int startInd=forSegIt->start_ind;
                unsigned int endInd=forSegIt->end_ind;
                typeof(M.find(rtn_name)) map_it=M.find(rtn_name);
                for(unsigned int ind=0;ind<2;ind++)
                {
                    string suspect=map_it->second[startInd].operands[ind];
                    bool flag=false;
                    for (unsigned int i=startInd+1;i<=endInd;i++)
                    {
                       if (map_it->second[i].opcode=="sub" || map_it->second[i].opcode=="add" )
                        if (map_it->second[i].operands[0]==suspect && map_it->second[i].operands[1].find("0x")!=string::npos)
                            flag=true;     
                    }
                    if (flag)
                    {
                        loopVar tmp;
                        /*if (suspect.find("[")!=string::npos)
                        {
                            int start=(suspect).find("[");
                            int end=(suspect).find("]");
                            tmp.name=(suspect).substr(start+1,end-start-1);
                        } */                       
                        tmp.name=suspect;
                        tmp.forSegIndex=forInd;
                        if (loopVarMap.find(rtn_name)==loopVarMap.end())
                        {
                            vector<loopVar> vec;
                            vec.push_back(tmp);
                            loopVarMap.insert(make_pair(rtn_name,vec));
                        }
                        else
                        {
                            typeof(loopVarMap.find(rtn_name)) loopVarMapIt=loopVarMap.find(rtn_name);
                            loopVarMapIt->second.push_back(tmp);
                        }
                    }
                }
                    forInd++;
            }
        }        
    }
    void buildAll()
    {
        segmentizeFor();
        segmentizeIf();
        //rectify();
        buildLocalMap();
        traverse(M,map_it)
            buildArrayLocalMap(map_it->first);
        buildNonForRegions();
        buildLoopVarMap();
        buildIfVarMap();
        buildParamMap();
    }
    long long int findCount(string adr)
    {
        traverse(insCount,it)
        {
            if(convertToInt(it->first)==convertToInt(adr))
                return it->second;
        }
        return -1;
    }

    VOID Fini(INT32 code, VOID *v)
    {
        buildAll();
        printRegions();
        printRegionByRegion();
        /*for (map<string,vector <instruction> > :: iterator map_it=M.begin();map_it!=M.end();map_it++)
    	{
    		cout<<endl<<"Routine Name: "<<map_it->first<<endl;
    		for(vector<instruction> :: iterator list_it=map_it->second.begin();list_it!=map_it->second.end();list_it++)
    		{
    			cout<<(list_it)->opcode<<" "<<list_it->adr<<".Count: "<<findCount(list_it->adr);
    			cout<<". Number of operands: "<<list_it->operands.size()<<". ";
    			for(unsigned int i=0;i<list_it->operands.size();i++)
    				cout<<"Operand "<<(i+1)<<" "<<list_it->operands[i]<<".";
    			cout<<endl;
                traverse(list_it->globals,globIt)
                    cout<<(globIt)->adr<<" "<<globIt->useType<<"  ";
                cout<<endl;
    		}
    		cout<<endl;
    	}
    	traverse(for_segment_map,map_it)
    	{
            string rtn_name=map_it->first;
    		cout<<endl<<"Routine Name: "<<rtn_name<<endl;
    		cout<<"Number of for loops is "<<map_it->second.size()<<endl;
            int i=0;
    		for(vector<segment> :: iterator it=map_it->second.begin();it!=map_it->second.end();it++,i++)
    		{
    			cout<<"For loop number "<<(i+1)<<" :";
    			cout<<"Start Index : "<<it->start_ind<<". End Index : "<<it->end_ind<<"."<<endl;
    			cout<<endl;
    		}
    		cout<<endl;
    	}
        //printRegions();*/
        traverse(loopVarMap,loopVarMapIt)
        {
            cout<<" ROUTINE NAME: "<<loopVarMapIt->first<<" Loop Vars: ";
            traverse(loopVarMapIt->second,listIt)
            {
                cout<<listIt->name<<" "<<listIt->forSegIndex<<" "<<endl;
            }
            cout<<endl;
        }
    	 traverse(if_segment_map,map_it)
    	{
            string rtn_name=map_it->first;
    		cout<<endl<<"Routine Name: "<<rtn_name<<endl;
    		cout<<"Number of if blocks is "<<map_it->second.size()<<endl;
            int i=0;
    		for(vector<ifSegment> :: iterator it=map_it->second.begin();it!=map_it->second.end();it++,i++)
    		{
    			cout<<"if block number "<<(i+1)<<" :";
    			cout<<"Start Index : "<<it->start_ind<<". End Index : "<<it->end_ind<<". Count "<<it->loopVarCount<<endl;
    			cout<<endl;
    		}
    		cout<<endl;
    	}
       /*traverse(local_map,lm_it)
        {
            cout<<"Routine Name: "<<lm_it->first<<" List of locals: \n";
            traverse(lm_it->second,lis_it)
            {
                cout<<" helo size helo size "<<lm_it->second.size()<<endl;
                traverse((*lis_it),it)
                {   cout<<"Local Name: "<<it->first<<" Size: "<<it->second.first<<endl;
                    cout<<"Usage: "<<endl;
                    traverse(it->second.second,it1)
                    {   
                        cout<<"Index: "<<it1->index<<"Type: "<<it1->useType<<endl;
                        cout<<endl;
                    }
                }
            }
            cout<<endl<<endl;
        }*/
        traverse(arrayLocal_map,map_it)
        {
            cout<<"Routine Name: "<<map_it->first<<" ";
            traverse(map_it->second,list_it)
            {
                cout<<"Instruction Index: "<<list_it->index<<". Parent Name: "<<list_it->parent<<". offset Index: "<<list_it->offsetIndex<<endl;
            }
        }
        /*traverse(rtnCount,count_it)
        {
            cout<<"Routine Name: "<<count_it->first<<". Call Count: "<<count_it->second<<endl;
        }*/
        traverse(paramMap,it)
        {
        	cout<<"Routine Name: "<<it->first;
        	cout<<". Number of Parameters obtained: "<<it->second.size()<<endl;
        	traverse(it->second,it1)
        		cout<<"Size :"<<it1->size<<". Is array: "<<it1->isArray<<"   ";
        	cout<<endl<<endl;

        }
        OutFile.close();
    }

    /* ===================================================================== */
    /* Print Help Message                                                    */
    /* ===================================================================== */

    INT32 Usage()
    {
        cerr << "This is the invocation pintool" << endl;
        cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
        return -1;
    }

    /* ===================================================================== */
    /* Main                                                                  */
    /* ===================================================================== */

    int main(int argc, char * argv[])
    {
        // Initialize pin & symbol manager
        if (PIN_Init(argc, argv)) return Usage();
        PIN_InitSymbols();

        // Register ImageLoad to be called to instrument instructions
        IMG_AddInstrumentFunction(ImageLoad, 0);
        INS_AddInstrumentFunction(Instruction, 0);
        RTN_AddInstrumentFunction(Routine, 0);
        PIN_AddFiniFunction(Fini, 0);

        OutFile.open(KnobOutputFile.Value().c_str());
        OutFile.setf(ios::showbase);

        // Start the program, never returns
        PIN_StartProgram();

        return 0;
    }




