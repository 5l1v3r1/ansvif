#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include "gzstream/gzstream.h"
#include <algorithm>
#include <vector>
#include <sys/time.h>
#include <random>
#include <cstdlib>
#include <climits>
#include <sstream>
#include <new>
#include "pstreams/pstream.h"
#include <cstring>
#include <sys/stat.h>
#include <thread>
#include <atomic>
#include <stdlib.h>
#include <iomanip>
#include <condition_variable>
#include <chrono>
#include <future>

/*
 * Marshall Whittaker / oxagast
 */

std::condition_variable cv;
std::mutex cv_m;
std::atomic<int> timer_a = ATOMIC_VAR_INIT(0);

void help_me(std::string mr_me) {
  std::cout
  << "Useage:" << std::endl
  << " " << mr_me << " -t template -c ./faulty -b 2048" << std::endl
  << "Options:" << std::endl
  << " -t [file]    This file should hold line by line command arguments" << std::endl
  << "              as shown in the example file." << std::endl
  << " -e [file]    This file should hold line by line environment variables" << std::endl
  << "              as shown in the example file.  You can" << std::endl
  << "              usually get these by doing something like:" << std::endl
  << "              $ strings /bin/mount | perl -ne 'print if /[A-Z]=$/' > mount_envs" << std::endl
  << " -c [path]    Specifies the command path." << std::endl
  << " -p [integer] Specifies the manpage location (as an integer, usually 1 or 8)." << std::endl
  << " -m [command] Specifies the commands manpage." << std::endl
  << " -f [integer] Number of threads to use.  Default is 2." << std::endl
  << " -b [integer] Specifies the buffer size to fuzz with." << std::endl
  << "              256-2048 Is usually sufficient."  << std::endl
  << " -r           Use only random data." << std::endl
  << " -z           Randomize buffer size from 1 to specified by the -b option." << std::endl
  << " -s \"@#^$CE\"  Characters to omit from randomization.  Default omitted" << std::endl
  << "              characters are: <>\\n |&\[]\()\{}: and mandatory omitted" << std::endl
  << "              characters are: >\\n" << std::endl
  << " -o [file]    Log to this file." << std::endl
  << " -v           Verbose." << std::endl
  << " -d           Debug." << std::endl;
  exit(0);
}


std::string remove_chars(const std::string& source, const std::string& chars) {
  std::string result="";
  for (unsigned int i=0; i<source.length(); i++) {
    bool foundany=false;
    for (unsigned int j=0; j<chars.length() && !foundany; j++) {
      foundany=(source[i]==chars[j]);
    }
    if (!foundany) {
      result+=source[i];
    }
  }
  return (result);
}


bool file_exists(const std::string& filen) {
  struct stat buf;
  return (stat(filen.c_str(), &buf) == 0);
}


int rand_me_plz (int rand_from, int rand_to) {
  std::random_device rd;
  std::default_random_engine generator(rd());   // seed random
  std::uniform_int_distribution<int> distribution(rand_from,rand_to);  // get a random
  auto roll = std::bind ( distribution, generator );  // bind it so we can do it multiple times
  return(roll());  
}


char fortune_cookie () {
  char chr;
  const char *hex_digits = "0123456789ABCDEF";
  int i;
  for( i = 0 ; i < 1; i++ ) {
    chr = hex_digits[ ( rand_me_plz(0, 255) ) ];
  }
  return(chr);
}


std::vector<std::string> get_flags_man(char* cmd, std::string man_loc, bool verbose, bool debug) {
  std::string cmd_name(cmd);
  std::string filename;
  std::vector<std::string> opt_vec;
  filename = "/usr/share/man/man" + man_loc + "/" + cmd_name + "." + man_loc + ".gz"; // put the filename back together
  if (file_exists(filename) == true) {
    char* chr_fn = strdup(filename.c_str()); // change the filename to a char pointer
    igzstream in(chr_fn);  //  gunzip baby
    std::string gzline;
    std::regex start_of_opt_1 ("^(\\.?\\\\?\\w{2} )*(\\\\?\\w{2} ?)*(:?\\.B )*((?:(?:\\\\-)+\\w+)(?:\\\\-\\w+)*).*"); // hella regex... why you do this to me manpage?
    std::smatch opt_part_1;
    std::regex start_of_opt_2 ("^\\.Op Fl (\\w+) Ar.*");
    std::smatch opt_part_2;
    std::regex start_of_opt_3 ("^\\\\fB(-.*)\\\\fB.*");
    std::smatch opt_part_3;
    while(std::getline(in, gzline)){
      if (std::regex_match(gzline, opt_part_1, start_of_opt_1)) {  // ring 'er out
        std::string opt_1 = opt_part_1[4];
        std::string opt_release = (remove_chars(opt_part_1[4], "\\"));  // remove the fucking backslashes plz
        opt_vec.push_back(opt_release);
      }
      if (std::regex_match(gzline, opt_part_2, start_of_opt_2)) {
        std::string opt_2 = opt_part_2[1];
        opt_vec.push_back("-" + opt_2);
      }
      if (std::regex_match(gzline, opt_part_3, start_of_opt_3)) {
        std::string opt_3 = opt_part_3[1];
        opt_vec.push_back(opt_3);
      }
    }
  }
  else {
    std::cerr << "Could not find a manpage for that command..." << std::endl;
    exit(1);
  }
  std::sort(opt_vec.begin(), opt_vec.end());
  opt_vec.erase(unique(opt_vec.begin(), opt_vec.end()), opt_vec.end());
  if (verbose == true) {
    std::cout << "Options being used: " << std::endl;
    for(int man_ln = 0; man_ln < opt_vec.size(); man_ln++) {  // loop around the options
      std::cout << opt_vec.at(man_ln) << " ";  // output options
    }
    std::cout << std::endl;
  }
  return(opt_vec);
}


std::vector<std::string> get_flags_template(std::string filename, bool verbose, bool debug) {
  int man_num;
  std::vector<std::string> opt_vec;
  std::string line;
  std::ifstream template_file (filename);
  if (template_file.is_open())
  {
    while (std::getline (template_file,line))
    {
      opt_vec.push_back(line);
    }
    template_file.close();
  }
  else { 
    std::cerr << "Could not open template file..." << std::endl;
    exit (1);
  }
  return(opt_vec);  
}


std::string trash_generator(int trash, int buf, std::string user_junk) {
  std::string junk = "";
  std::string hex_stuff;
  int trash_num;
  if (trash == 0) {                                            // kosher
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = "A" + junk; // put lots of As
    }
  }
  if (trash == 1) {
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = "9" + junk; // yadda yadda
    }
  }
  if (trash == 2) {
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = junk += fortune_cookie();
    }
  }
  if (trash == 3) {                                            // front
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = "A" + junk; // put lots of As
    }
    junk = user_junk + junk;
    if (buf-user_junk.length() < junk.size()) junk = junk.substr(0,buf);
    else return ("OOR");
  }
  if (trash == 4) {
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = "9" + junk; // yadda yadda
    }
    junk = user_junk + junk;
    if (buf-user_junk.length() < junk.size()) junk = junk.substr(0,buf);
    else return ("OOR");
  }
  if (trash == 5) {
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = junk += fortune_cookie();
    }
    junk = user_junk + junk;
    if (buf-user_junk.length() < junk.size()) junk = junk.substr(0,buf);
    else return ("OOR");
  }
  if (trash == 6) {
    for (trash_num = 0; trash_num < buf; trash_num++) {  // back
      junk = "A" + junk; // put lots of As
    }
    junk = junk + user_junk;
    if (buf-user_junk.length() < junk.size()) junk = junk.substr(junk.length()-buf);
    else return ("OOR");
  }
  if (trash == 7) {
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = "9" + junk; // yadda yadda
    }
    junk = junk + user_junk;
    if (buf-user_junk.length() < junk.size()) junk = junk.substr(junk.length()-buf);
    else return ("OOR");
  }
  if (trash == 8) {
    for (trash_num = 0; trash_num < buf; trash_num++) {
      junk = junk += fortune_cookie();
    }
    junk = junk + user_junk;
    if (buf-user_junk.length() < junk.size()) junk = junk.substr(junk.length()-buf);
    else return ("OOR");
  }
  
  return(junk);
}



std::string make_garbage(int trash, int buf) {
  buf = buf-1;
  std::string all_junk;
  if (isatty(STDIN_FILENO)) {
    std::string user_stuff = "";
    all_junk = trash_generator(trash, buf, user_stuff);
  }
  else {
    std::string user_stuff;
    getline(std::cin, user_stuff);
    all_junk = trash_generator(trash, buf, user_stuff);
  }
  return(all_junk);
}


std::string execer(std::string the_cmd_str, bool verbose, bool debug, std::string write_file_n) {
  std::vector<std::string> errors;
  redi::ipstream in(the_cmd_str + " >&2", redi::pstreambuf::pstderr); // gotta put them to stderr
  std::string errmsg;                                                      // some os thing
  while (std::getline(in, errmsg)) {
    if (debug == true) {
      std::cout << errmsg << std::endl;
      std::ofstream w_f;
      w_f.open (write_file_n, std::ios::out | std::ios::app);
      w_f << errmsg << std::endl;
      w_f.close();
    }
    errors.push_back(errmsg);
  }
  if (errors.size() > 0) {
    return (errors[errors.size() - 1]);
  }
  else return ("NOSEG");                                                // damn it...
}


std::string binstr_to_hex(std::string bin_str) {
  std::stringstream hex_out;
  hex_out << std::setw(2) << std::hex << std::uppercase;
  std::copy(bin_str.begin(), bin_str.end(), std::ostream_iterator<unsigned int>(hex_out, "\\"));
  std::string hexxy = "\\" + hex_out.str();
  return (hexxy);
}


void write_seg(std::string filename, std::string seg_line) {
  std::ofstream w_f;
  w_f.open (filename, std::ios::out | std::ios::app);
  w_f << seg_line << std::endl;
  w_f.close();
}


bool match_seg(int buf_size, std::vector<std::string> opts, std::vector<std::string> spec_env, std::string path_str, std::string strip_shell, bool rand_all, bool write_to_file, std::string write_file_n, bool rand_buf, bool verbose, bool debug) {
  bool segged = false;
  if (file_exists(path_str) == true) {
    while (segged == false) {
      int rand_spec_one, rand_spec_two;
      if (rand_all == true) {
        rand_spec_one = 2;
        rand_spec_two = 2;
      }
      else {
        rand_spec_one = 0;
        rand_spec_two = 8;
      }
      std::vector<std::string> junk_opts_env;
      std::vector<std::string> junk_opts;
      std::string env_str;
      std::string sys_str;
      for(int cmd_flag_l = 0; cmd_flag_l < opts.size(); cmd_flag_l++) {  // loop around the options
        if (rand_me_plz(0,1) == 1) {   // roll tha die
          junk_opts.push_back(opts.at(cmd_flag_l));  // put the random arg in the vector
        }
      }
      for(int cmd_flag_a = 0; cmd_flag_a < spec_env.size(); cmd_flag_a++) {  // loop around the options
        if (rand_me_plz(0,1) == 1) {   // roll tha die
          junk_opts_env.push_back(spec_env.at(cmd_flag_a));  // put the random arg in the vector
        }
      }
      if (rand_buf == true) {
        for( std::vector<std::string>::const_iterator junk_opt_env = junk_opts_env.begin(); junk_opt_env != junk_opts_env.end(); ++junk_opt_env) { // loop through the vector of junk envs
          std::string oscar_env = remove_chars(make_garbage(rand_me_plz(rand_spec_one,rand_spec_two),rand_me_plz(1,buf_size)), strip_shell);
          if (oscar_env != "OOR") {
            env_str = env_str + *junk_opt_env + oscar_env + " ";
          }
        }
        
        for( std::vector<std::string>::const_iterator junk_opt = junk_opts.begin(); junk_opt != junk_opts.end(); ++junk_opt) { // loop through the vector of junk opts
          std::string oscar = remove_chars(make_garbage(rand_me_plz(rand_spec_one,rand_spec_two),rand_me_plz(1,buf_size)), strip_shell);
          if (oscar != "OOR") {
            sys_str = sys_str + *junk_opt + " " + oscar + " ";  // add options and garbage
          }
        }
      }
      else if (rand_buf == false) {
        
        for( std::vector<std::string>::const_iterator junk_opt_env = junk_opts_env.begin(); junk_opt_env != junk_opts_env.end(); ++junk_opt_env) { // loop through the vector of junk envs
          std::string oscar_env = remove_chars(make_garbage(rand_me_plz(rand_spec_one,rand_spec_two),buf_size), strip_shell);
          if (oscar_env != "OOR") {
            env_str = env_str + *junk_opt_env + oscar_env + " ";
          }
        }
        for( std::vector<std::string>::const_iterator junk_opt = junk_opts.begin(); junk_opt != junk_opts.end(); ++junk_opt) { // loop through the vector of junk opts
          std::string oscar = remove_chars(make_garbage(rand_me_plz(rand_spec_one,rand_spec_two),buf_size), strip_shell);
          if (oscar != "OOR") {
            sys_str = sys_str + *junk_opt + " " + oscar + " ";  // add options and garbage
          }
        }
      }
      std::string out_str = env_str + " " + path_str + " " + sys_str;
      junk_opts.clear();
      junk_opts.shrink_to_fit();
      junk_opts_env.clear();
      junk_opts_env.shrink_to_fit();
      if (debug == true) {
        std::ofstream w_f;
        w_f.open (write_file_n, std::ios::out | std::ios::app);
        w_f << out_str << std::endl;
        w_f.close();
        std::cout << std::endl << out_str << std::endl;
      }
      std::istringstream is_it_segfault(execer(out_str, verbose, debug, write_file_n)); // run it and grab stdout and stderr
      std::string sf_line;
      while (std::getline(is_it_segfault, sf_line)) {
        std::regex sf_reg ("(.*Segmentation fault.*|.*core dump.*)"); // regex for the sf
        std::smatch sf;
        if (regex_match(sf_line, sf, sf_reg)) {  // match segfault
          std::cout << "Segfaulted with: " << out_str << std::endl;
          if (write_to_file == true) {
            write_seg(write_file_n, out_str);
            std::cout << "Segmentation fault logged" << std::endl;
          }
          return(true);
        }
      }
    }
  }
  else {
    std::cerr << "Command not found at path..." << std::endl;
    exit(1);
  }
}


int coat (int id, int buf_size_int, std::vector<std::string> opts, std::vector<std::string> spec_env, std::string path_str, std::string strip_shell, bool rand_all, bool write_to_file, std::string write_file_n, bool rand_buf, bool verbose, bool debug) {
  std::future<bool> fut = std::async (match_seg, buf_size_int, opts, spec_env, path_str, strip_shell, rand_all, write_to_file, write_file_n, rand_buf, verbose, debug);
  std::chrono::milliseconds span (100);
  while (fut.wait_for(span) == std::future_status::timeout) std::cout << '.';
  fut.get();
  exit(0);
}


int main (int argc, char* argv[]) {
  char* man_chr;
  int opt;
  int num_threads = 2;
  std::vector<std::string> opts;
  std::vector<std::string> spec_env;
  std::string man_loc = "8";
  std::string buf_size;
  std::string mp;
  std::string template_file;
  std::string strip_shell = "<>\n |&\[]\()\{}:";
  std::string u_strip_shell;
  std::string write_file_n = "";
  std::string path_str = "";
  bool template_opt = false;
  bool man_opt = false;
  bool rand_all = false;
  bool rand_buf = false;
  bool write_to_file = false;
  bool u_strip_shell_set = false;
  bool verbose = false;
  bool debug = false;
  while ((opt = getopt(argc, argv, "m:p:t:e:c:f:o:b:s:hrzvd")) != -1) {
    switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 'd':
        debug = true;
        break;
      case 't':
        template_opt = true;
        template_file = optarg;
        break;
      case 'c':
        path_str = optarg;
        break;
      case 'b':
        buf_size = optarg;
        break;
      case 'e':
        spec_env = get_flags_template(optarg, verbose, debug);
        break;
      case 'p':
        man_loc = optarg;
        break;
      case 'm':
        man_opt = true;
        man_chr = optarg;
        break;
      case 'f':
        num_threads = std::atoi(optarg);
        break;
      case 'o':
        write_to_file = true;
        write_file_n = optarg;
        break;
      case 'h':
        help_me(argv[0]);
        break;
      case 'r':
        rand_all = true;
        break;
      case 'z':
        rand_buf = true;
        break;
      case 's':
        u_strip_shell = optarg;
        u_strip_shell_set = true;
        break;
      default:
        help_me(argv[0]);
    }  
  }
  if (u_strip_shell_set == true) {
    strip_shell = u_strip_shell + ">\n";
  }
  if ((man_opt == true) && (template_opt == false)) {
    opts = get_flags_man(man_chr, man_loc, verbose, debug);
  }
  else if ((man_opt == false) && (template_opt == true)) {
    opts = get_flags_template(template_file, verbose, debug);
  }
  else if ((man_opt == true) && (template_opt == true)) {
    help_me(argv[0]);
  }
  else if ((man_opt == false) && (template_opt == false)) {
    help_me(argv[0]);
  }
  else if (path_str == "") {
    help_me(argv[0]);
  }
  else {
    help_me(argv[0]);
  }
  std::istringstream b_size(buf_size);
  int is_int;
  if (!(b_size >> is_int)) {
    help_me(argv[0]);
  }
  char buf_char_maybe;
  if (b_size >> buf_char_maybe) {
    help_me(argv[0]);
  }
  else {
    int buf_size_int = std::stoi(buf_size);
    std::vector<std::thread> threads;
    for (int cur_thread=1; cur_thread <= num_threads; ++cur_thread) threads.push_back(std::thread(coat, cur_thread, buf_size_int, opts, spec_env, path_str, strip_shell, rand_all, write_to_file, write_file_n, rand_buf, verbose, debug));  // Thrift Shop
    for (auto& all_thread : threads) all_thread.join();  // is that your grandma's coat?
    exit(0);
  }
}