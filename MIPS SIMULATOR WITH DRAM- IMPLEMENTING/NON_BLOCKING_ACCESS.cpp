#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <queue>
using namespace std;
int n = -1 ;   // program counter
int clock_cycles = 0 ;
int buffer_updates  = 0 ;
int ROW_ACCESS_DELAY = 10 , COLUMN_ACCESS_DELAY = 2;
const int Max_instruction_addr = (1 << 16) ;
const int Max_data_addr =(1<<18)-1;
map<string,int> lab_addr , lab_called;
vector<int>Register;
vector<int>hold_count;
map<string,int> instruction_count;
bool word_addressability_violated = false;

struct mem{
    int inst ;      //  inst=0 indicates data is stored , inst=1 indicated instruction is stored
    string kind; // stores type of instruction
    string label; // stores label in case of bne,beq and j
    int args[3]; // stores appropriate register number,
    //  offset (incase of sw and lw), constant (addi)
    // and 32 bit signed integer in case of data
};

vector < vector < mem > > DRAM ;
//   vector<mem>Memory;
int current_row_in_BUFFER = -1;
vector<mem> ROW_BUFFER;


int busy_dram = 0 ;
int t_curr = 0   ;
queue < pair <int , mem >  > Q ;

mem Memory(int x)
{
    if(x >= (1<<16)) throw "Invalid instruction location";
    return DRAM[x/256][x%256];
}

void store_instructions(int x, mem y)
{
    DRAM[x/256][x%256] = y;
}

void copy_to_dram()
{
    DRAM[current_row_in_BUFFER] = ROW_BUFFER;
    t_curr+=ROW_ACCESS_DELAY;
    clock_cycles+=ROW_ACCESS_DELAY;
}

void copy_to_buffer(int x)
{
    if(x != current_row_in_BUFFER)
    {   cout<<clock_cycles+1<<"-";
        if(current_row_in_BUFFER != -1)copy_to_dram();
        buffer_updates++;
        ROW_BUFFER = DRAM[x];
        current_row_in_BUFFER = x ; //cout<<x<<"\n";
        clock_cycles+=ROW_ACCESS_DELAY;
        t_curr += ROW_ACCESS_DELAY;
        cout<<clock_cycles<<":"<<"Buffer updated to row "<<x<<" of DRAM\n\n";
    }
}


int read(int x)
{
    
    copy_to_buffer(x/256);
    cout<<clock_cycles+1<<"-";
    t_curr+=COLUMN_ACCESS_DELAY;
    clock_cycles+=COLUMN_ACCESS_DELAY;
    return ROW_BUFFER[x%256].args[0];
}

void write(int x , int z)
{
    // copy_to_dram();
    copy_to_buffer(x/256);
    cout<<clock_cycles+1<<"-";
    t_curr+=COLUMN_ACCESS_DELAY;
    clock_cycles+=COLUMN_ACCESS_DELAY;
    ROW_BUFFER[x%256].args[0]= Register[z];
    cout<<clock_cycles<<":"<<"Row buffer corresponding to DRAM row "<<x/256<<" at column "<<x%256<<" is updated to "<<Register[z]<<"\n\n";
}
// initialises memory and register contents

void complete_dram_activity()
{
    t_curr = 0 ;
    int x = clock_cycles;
    if(!Q.empty())
    {
        mem x = Q.front().second;
        string kind = x.kind;
        if(Register[x.args[2]]%4 !=0 or x.args[1]%4 !=0) throw "Not word addressable";
        int mem_loc = x.args[1]/4+Max_instruction_addr+Register[x.args[2]]/4;
        if(kind == "sw")
        {
            clock_cycles = max(Q.front().first,busy_dram);
            write(mem_loc,Q.front().second.args[0]);
            
            busy_dram = max(Q.front().first +t_curr , busy_dram + t_curr);
            //clock_cycles = max(clock_cycles,busy_dram);
        }
        else
        {   clock_cycles = max(Q.front().first,busy_dram);
            Register[x.args[0]] = read(mem_loc);
            cout<<clock_cycles<<":"<<"Register updated from buffer: "<<"$r"<<x.args[0]<<"="<<Register[x.args[0]]<<"\n\n";
            busy_dram = max(Q.front().first +t_curr , busy_dram + t_curr);
            //clock_cycles = max(clock_cycles,busy_dram);
            
        }
        hold_count[x.args[0]]--;
        hold_count[x.args[2]]--;
        Q.pop();
    }
    clock_cycles = x ;
}

void initialise()
{
    DRAM.resize(1<<10, vector<mem>(1<<8));
    // cout<<"here2\n";
    // Memory.resize(1<<18);
    Register.resize(32,0);
    hold_count.resize(32,0);
}


// checks that the called labes in bne,beq and j actually exist
void check_labels()
{
    for(auto u : lab_called){
        if(lab_addr.find(u.first)==lab_addr.end())
        {
            throw "label not defined";
        }
    }
}


// returns hex digit corresponding to integer x
char hex_digit(int x)
{
    if(x < 10) return x+'0';
    else if(x == 10) return 'A';
    else if(x == 11) return 'B';
    else if(x == 12) return 'C';
    else if(x == 13) return 'D';
    else if(x == 14) return 'E';
    return 'F';
}


//converts decimal to hex
string dec_to_hex(int x )
{
    string s ="";
    
    unsigned int y = (unsigned int) x ;
    while(y>0)
    {
        s.push_back(hex_digit(y%16));
        y/=16;
    }
    while(s.length()<8){
        s.push_back('0');
    }
    reverse(s.begin(),s.end());
    
    return s ;
}


// prints register values after execution of each instruction
void print_instruction(mem instruction)
{
    string kind = instruction.kind;
    if(kind == "add" or kind =="sub" or kind == "mul" or kind == "slt")
    {
        cout<<kind<<" $r"<<instruction.args[0]<<",$r"<<instruction.args[1]<<",$r"<<instruction.args[2];
    }
    else if(kind == "lw" or kind == "sw")
    {
        cout << kind << " $r"<<instruction.args[0]<<","<<instruction.args[1]<<"($r"<<instruction.args[2]<<")";
    }
    else if(kind == "addi")
    {
        cout<<kind<<" $r"<<instruction.args[0]<<",$r"<<instruction.args[1]<<","<<instruction.args[2];
    }
    else if(kind == "j")
    {
        cout << kind <<" "<<instruction.label;
    }
    else if(kind == "beq" or kind == "bne")
    {
        cout<<kind<<" $r"<<instruction.args[0]<<",$r"<<instruction.args[1]<<","<<instruction.label;
    }
}

/*void print_reg_values(int pc)
{
    cout<<"Register values, after executing instruction at address : "<<pc<<":"<<clock_cycles<< " : "<<endl;
    cout<<"$zero : "<<dec_to_hex(Register[0])<<endl;
    
    for(int i=1;i<32;i++)
        cout<<"$r"<<i<<" : "<<dec_to_hex(Register[i])<<endl;
}*/


void get_ready(mem x)
{
    string kind = x.kind ;
    if(kind == "add" or kind == "sub" or kind == "mul" or kind == "slt")
    {
        while(!(hold_count[x.args[0]]==0 and hold_count[x.args[1]]==0 and hold_count[x.args[2]]==0))
        {
            complete_dram_activity();
            clock_cycles = max(clock_cycles,busy_dram);
        }
    }
    if(kind == "beq" or kind == "bne" or kind == "addi")
    {
        while(!(hold_count[x.args[0]]==0 and hold_count[x.args[1]]==0 ))
        {
            complete_dram_activity();
            clock_cycles = max(clock_cycles,busy_dram);
        }
    }
}
// function to interpret the stored instructions and execute them
void evaluate()
{
    int i = 0 ;
    string s ;
    // int j = 0 ;
    while(i <= n)
    {
        // clock_cycles++;
        // s = "";
        //cout<<"here2\n";
       // print_reg_values(i) ; //print register values after executing ith iteration
        
        string kind = Memory(i).kind;
        
        get_ready(Memory(i));
        
         clock_cycles++;
        
        instruction_count[kind]++;
        
        
        
        if(kind == "add") {
            Register[Memory(i).args[0]] = Register[Memory(i).args[1]]+Register[Memory(i).args[2]] ;
        }
        
        else if(kind == "sub")
        {
            Register[Memory(i).args[0]] = Register[Memory(i).args[1]]-Register[Memory(i).args[2]] ;
            
        }
        
        
        else if(kind == "mul") Register[Memory(i).args[0]] = Register[Memory(i).args[1]] * Register[Memory(i).args[2]] ;
        
        else if(kind == "addi") Register[Memory(i).args[0]] = Register[Memory(i).args[1]] + Memory(i).args[2] ;
        
        else if(kind == "lw") {
            
            cout << clock_cycles<<":" ; print_instruction(Memory(i)) ; cout << ": DRAM request initiated\n\n";
            int mem_loc=Memory(i).args[1]+Max_instruction_addr+Register[Memory(i).args[2]]/4;  // memory address from which value has to be loaded
            if(mem_loc>Max_data_addr || mem_loc<0 || mem_loc<Max_instruction_addr)
                throw "Data Memory Overflow";
            hold_count[Memory(i).args[0]]++;
            if(Memory(i).args[0]!=Memory(i).args[2])hold_count[Memory(i).args[2]]++;
            Q.push({clock_cycles,Memory(i)});
            //Register[Memory(i).args[0]] = read(mem_loc);
            //cout<<clock_cycles<<":"<<"Register updated from buffer: "<<"$r"<<Memory(i).args[0]<<"="<<Register[Memory(i).args[0]]<<"\n\n";
            /*Register[Memory[i].args[0]] = Memory[mem_loc].args[0];*/
        }
        
        else if(kind == "sw") {
            cout << clock_cycles<<":"; print_instruction(Memory(i)) ; cout << ": DRAM request initiated\n\n";
            int mem_loc=Memory(i).args[1]+Max_instruction_addr+Register[Memory(i).args[2]]/4; // memory address where which value has  to be stored
            if(mem_loc>Max_data_addr || mem_loc<0 || mem_loc<Max_instruction_addr)
                throw "Data Memory Overflow";
            hold_count[Memory(i).args[0]]++;
            if(Memory(i).args[0]!=Memory(i).args[2])hold_count[Memory(i).args[2]]++;
            Q.push({clock_cycles,Memory(i)});
            //write(mem_loc , Memory(i).args[0]);
            
            /*Memory[mem_loc].args[0] = Register[Memory[i].args[0]];*/
            
            
        }
        
        else if(kind == "beq") {if(Register[Memory(i).args[0]] == Register[Memory(i).args[1]]) {cout << clock_cycles <<":";print_instruction(Memory(i));cout<<"\n\n";i = lab_addr[Memory(i).label]; continue;}}
        
        else if(kind == "bne") {if(Register[Memory(i).args[0]] != Register[Memory(i).args[1]]) {cout << clock_cycles <<":";print_instruction(Memory(i));cout<<"\n\n";i = lab_addr[Memory(i).label]; continue;}}
        
        else if(kind == "j") {cout << clock_cycles <<":";print_instruction(Memory(i));cout<<"\n\n";i = lab_addr[Memory(i).label]; continue ; }
        
        else if(kind == "slt") Register[Memory(i).args[0]]  = (Register[Memory(i).args[1]]< Register[Memory(i).args[2]]) ;
        
        if(kind != "lw" and kind != "sw")
        {
            cout << clock_cycles <<":";print_instruction(Memory(i));
            if(kind != "bne" and kind != "beq" and kind !="j" )cout<<":" << "$r"<<Memory(i).args[0]<<"="<<Register[Memory(i).args[0]];
            cout<<"\n\n";
        }

        i++;
    }
    while(!Q.empty()) complete_dram_activity();
    clock_cycles = max(clock_cycles,busy_dram);
    // print_reg_values(i) ; //print register values after executing last iteration
}

// converts register name to its corresponding number
int string_to_reg(string s)
{
    if(s=="$zero") return 0;   // if s is zero reg
    
    if(s[0] != '$' || s.size() <= 2 || s[1]!='r') return -1;
    
    int x = 0 ;
    for(int i = 2 ; i < s.length() ;i++) {
        if((s[i]-'0')<=9 && (s[i]-'0')>=0)
            x = x*10+(s[i]-'0');
        else return -1;
    }
    if(x >= 32 or x <= 0) return -1;
    return x ;
}

// converts number stored in string to int type
pair<int,int> string_to_int(string s){
    int x=0;
    bool is_neg=0;
    
    if(s.size()==0)
        return make_pair(x,0);
    
    int i=0;
    
    if(s[0]=='-'){
        is_neg=1;
        i++;
    }
    for(;i < s.length() ; i++){
        if((s[i]-'0')<=9 && (s[i]-'0')>=0)
            x=x*10+(s[i]-'0');
        else return make_pair(x,0);
    }
    if(is_neg==1)
        x=-x;
        
    return make_pair(x,1);
}

//checks whether label name is valid identifier
int valid_label_name(string s){
    if(((s[0]-'A'<0) || (s[0]-'A'>25)) && ((s[0]-'a'<0) || (s[0]-'a'>25)))
        return -2;
    for(int i=1;i<s.length();i++){
        if(((s[i]-'A'<0) || (s[i]-'A'>25)) && ((s[i]-'a'<0) || (s[i]-'a'>25)) && ((s[i]-'0'<0) || (s[i]-'0'>9)))
            return -2;
    }
    return 1;
}
// reads the instruction stored in vector v and stores it in memory if its a valid instructions
int read_instruction(vector<string>v)
{
    n++;        //increment pc
   // for(auto x : v) cout << x <<" " ; cout<<"\n";
    if(n>Max_instruction_addr)
        throw "Instruction Memory Overflow";
    
    if(v.size() == 1){
        if(v[0].size()>=2 && v[0][v[0].size()-1] == ':')
        {
            string label_name = v[0].substr(0,v[0].size()-1);
            // check uniqueness of label
            // also check if label has to be added to memory
            if(lab_addr.find(label_name)==lab_addr.end()) {
                lab_addr[label_name] = n;
                //     label_lines++;
                n--;
                return valid_label_name(label_name);
            }
            else return false;
        }
        else return false;
    }
    
    else if(v.size() == 2)
    {
        if(v[1] == ":")
        {
            // check uniqueness of label
            
            // also check if label has to be added to memory
            if(lab_addr.find(v[0])==lab_addr.end()) {
                lab_addr[v[0]] = n;
                //   label_lines++;
                 n--;
                return valid_label_name(v[0]);;
            }
            else return false;
        }
        else if(v[0]=="j")
        {
            store_instructions(n,{1,"j",v[1]});
            // Memory(n).kind = "j" ; Memory(n).inst = 1 ; Memory(n).label = v[1];
            lab_called[v[1]]=1; //indicates presence of branching instruction to v[i]
            return true;
        }
        else return false;
    }
    
    else if(v.size() == 4)
    {
        string kind = v[0];
        if(kind == "beq" or kind == "bne")
        {
            int x = string_to_reg(v[1]) ;
            int y = string_to_reg(v[2]);
            if(x!=-1 and y!=-1)
            {
                store_instructions(n,{1,kind,v[3],{x,y,-1}})  ;
                lab_called[v[3]]=1;
                return true;
            }
            else return false;
        }
        else if( kind == "add" or (kind == "sub") or (kind == "mul") or (kind == "slt"))
        {
            int x = string_to_reg(v[1]);
            int y = string_to_reg(v[2]);
            int z = string_to_reg(v[3]);
            
            
            if(x==0) return -1;  // zero reg value cant be changed
            
            else if(x!=-1 and y!=-1 and z!=-1)
            {
                store_instructions(n,{1,kind,"",{x,y,z}});
                return true;
            }
            else return false;
        }
        else if(kind == "addi" ){
            int x = string_to_reg(v[1]);
            int y = string_to_reg(v[2]);
            auto z = string_to_int(v[3]);
            
            if(x==0) return -1;
            else if(x!=-1 and y!=-1 and z.second!=0)
            {
                store_instructions(n,{1,kind,"",{x,y,z.first}}); // reg1,reg2,const
                return true;
            }
            else return false;
        }
        else return false;
    }
    
    else if(v.size() == 3 )
    {
        string kind = v[0];
        if(kind == "lw" or kind == "sw")
        {
            int x = string_to_reg(v[1]);
            if(x<=0) return -1;   // zero reg value cant be changed
            
            vector<string> parts;   string delimiters(" ( )");
            boost::split(parts, v[2], boost::is_any_of(delimiters));

            if(parts.size()== 3 && x!=-1){

                auto y = string_to_int(parts[0]);
                int z = string_to_reg(parts[1]);
                
                if(y.second == 1  && z!=-1 && parts[2].size()==0){
                    // if(y.first%4 != 0) return false;
                    store_instructions(n,{1,kind,"",{x,y.first,z}});
                    return true;
                }
                else return false;
            }
            else return false;
        }
        else return false;
    }

    return false;
}



int main(int argc , char** argv)
{
    //cout<<"here1\n";
    ifstream file(argv[1]);
    if(argc >= 3) ROW_ACCESS_DELAY = stoi(argv[2]);
    if(argc >= 4) COLUMN_ACCESS_DELAY = stoi(argv[3]);
    string line, delimiters(" \" \", ");
    
    //initialise memory and register
    initialise();
    
    vector<string>instructions = {"add" , "sub" ,"addi" ,"mul" ,"bne" ,"beq" ,"j" ,"sw" ,"lw" ,"slt"};
    for(string s : instructions) instruction_count[s]=0;
    
    while (std::getline(file, line))
    {
        vector<string> parts,parts_1;
        boost::split(parts, line, boost::is_any_of(delimiters));
        
        
        for(auto u : parts)
        {
            if(u.size()!=0 && u!="\n" && u!="\t")
                parts_1.push_back(u);
        }
        
        // if parts_1 is empty than continue
        if(parts_1.size()==0)
            continue;
        

        parts_1[0].erase(remove(parts_1[0].begin(),parts_1[0].end(), ' '), parts_1[0].end());  // remove white space from 1st field
        parts_1[0].erase(remove(parts_1[0].begin(),parts_1[0].end(), '\t'), parts_1[0].end()); // remove tabs from 1st field
        
        int f = read_instruction(parts_1);
        //cout<<"here3\n";
        if(f==0)
        {
            cout<<"Syntax Error"<<"\n";
            return 0;
        }
        if(f==-1)
        {
            cout<<"zero register's value can't be changed"<<"\n";
            return 0;
        }
        if(f==-2)
        {
            cout<<"invalid label name"<<"\n";
            return 0;
        }
        //cout<<"here4\n";
    }
    
    file.close();
    //cout<<"here5\n";
    // output file
    freopen("output_NBA.txt","w", stdout);
    //cout<<"here2\n";
    try{
     check_labels();
    }catch (const char* msg) {
        cerr << msg << "\n";
        return 0;
    }

    try{
     evaluate();
    }catch (const char* msg) {
        cerr << msg <<"\n";
        return 0;
    }
    
    cout<<"Number of clock cycles :"<<clock_cycles<<"\n";
    cout<<"Number of buffer updates :"<<buffer_updates<<"\n";
    cout<<"Instruction type : Frequency\n";
    for(auto x : instruction_count) cout <<x.first << ":" << x.second<<"\n";
    
}







