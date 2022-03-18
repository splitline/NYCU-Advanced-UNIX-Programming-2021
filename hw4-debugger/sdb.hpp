#include <sys/user.h>

#include <functional>
#include <map>
#include <string>
#include <vector>
using namespace std;

typedef unsigned long long ull;

#define state_constraint(s)                               \
    if (state != s) {                                     \
        cerr << "** Error: This command is for "          \
             << string(#s).substr(10) << " program only." \
             << endl;                                     \
        return;                                           \
    }

enum SdbState {
    init,
    loaded,
    running
};

struct Breakpoint {
    int id;
    ull address;
    unsigned char original;
};

class Sdb {
   private:
    SdbState state = SdbState::init;
    string program;
    pid_t pid;
    ull entry_address;
    map<string, function<void(vector<string>)>> command_functions;

    map<int, Breakpoint> bp_id_map;
    map<ull, Breakpoint> bp_addr_map;
    int bpid = 0;
    bool hit_bp = false;

    void load(vector<string>);
    void start(vector<string>);
    void run(vector<string>);
    void help(vector<string>);

    void get_reg(vector<string>);
    void set_reg(vector<string>);
    void dump_regs(vector<string>);
    void disasm(vector<string>);
    void vmmap(vector<string>);
    void dump_mem(vector<string>);

    void set_bp(vector<string>);
    void remove_bp(vector<string>);
    void list_bp(vector<string>);
    void single_step(vector<string>);
    void cont(vector<string>);

    long get_code(ull address);
    void check_bp();
    map<string, ull*> fetch_regs();
    struct user_regs_struct regs_struct;

   public:
    Sdb();
    ~Sdb();
    void launch(string program, string script_path);
};
