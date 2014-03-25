#include "executor.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <dlfcn.h>
#include <cstdlib>


#include <Inventor/nodes/SoSeparator.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>

#define NO_PLUGIN_FOUND "ro.utcluj.aut.nopluginfound"

using namespace std;

typedef void (*executor_plugin)(std::string &&, SoSeparator *, std::string name);

string decode_response_utf8(string &SRC) {
    string ret;
    char ch;
    int i, ii;
    for (i=0; i<SRC.length(); i++) {
        if (int(SRC[i])==37) {
            sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            ret+=ch;
            i=i+2;
        } else if(int(SRC[i])==43) {
	    ret+=" ";
	} else {
            ret+=SRC[i];
        }
    }
    return (ret);
}



struct plugin_info
{

  std::string plugin_name;
  std::string plugin_source;
 
  void read(std::stringstream & is);

};

void plugin_info::read( std::stringstream & is )
{

  using boost::property_tree::ptree;
  ptree pt;

  read_xml(is, pt);

  plugin_name = pt.get<std::string>("response.client_plugin");
  plugin_source = pt.get<std::string>("response.client_plugin_source");
  
}

std::string select_execution_plugin(std::string message, std::string & source)
{

  try
    {
      plugin_info p;
      stringstream streammed_msg;
      streammed_msg<<message;
      p.read(streammed_msg);
      
      std::cout<<std::endl<<"PLUGIN "<<p.plugin_name<<std::endl<<" SOURCE "<<p.plugin_source<<std::endl;
      source = p.plugin_source;

      return p.plugin_name;
      
    }
  catch(std::exception &e)
    {
      std::cout<<std::endl<<e.what()<<std::endl;
      return NO_PLUGIN_FOUND;
    }

  
  return NO_PLUGIN_FOUND;
}



command_executor::command_executor() 
{

}

command_executor::command_executor(std::string exec_name) 
{
  name = exec_name;
  vrml_node = NULL;

}

command_executor::~command_executor()
{
}


void command_executor::set_vrml_tree_node(SoSeparator *node)
{
  vrml_node = node;
}

void command_executor::onMessageReceived(std::string message)
{

  std::cout<<std::endl<<decode_response_utf8(message);
  std::string src_plugin_ ="";
  std::string plug_in_name = select_execution_plugin(decode_response_utf8(message), src_plugin_);
  
  if(plug_in_name != NO_PLUGIN_FOUND)
    {
      
      executor_plugin plug_in;
      void *lib_handle;
      char *error;

      lib_handle = dlopen(plug_in_name.c_str(), RTLD_LAZY);
      if (!lib_handle) 
	{
	  std::cout<<std::endl<<dlerror();
	  std::cout<<std::endl<<"****** TRY TO DOWNLOAD *******"<<std::endl
		   <<src_plugin_
		   <<std::endl<<"******************************"<<std::endl;
  
	  std::string wget_cmd = "wget -P ./plugins/ "+src_plugin_; 
	  system(wget_cmd.c_str());

	  plug_in_name.erase(0,10);
	  std::cout<<std::endl<<"OML "<<plug_in_name;
	  std::string link1 = plug_in_name+".1";
	  std::string link2 = link1+".0";
	  std::cout<<std::endl<<"OML "<<link1;
	  //ln -sf /opt/lib/libctest.so.1.0 /opt/lib/libctest.so.1
	  //ln -sf /opt/lib/libctest.so.1.0 /opt/lib/libctest.so
	  std::string cmd1 = "./make_links.sh "+link2+" "+link1;
	  std::string cmd2 = "./make_links.sh "+link2+" "+plug_in_name;
	  std::cout<<std::endl<<"OML "<<cmd1;
	  std::cout<<std::endl<<"OML "<<cmd2;
	  
	  system(cmd1.c_str());
	  system(cmd2.c_str());
	  

	  return ;
	}

      plug_in = (executor_plugin)dlsym(lib_handle, "l3d_client_plugin");
      if ((error = dlerror()) != NULL)  
	{
	  std::cout<<std::endl<<error;
	  return ;
	}

      if((vrml_node!=NULL))
	{
	  plug_in(std::move(decode_response_utf8(message)), vrml_node, name);
	}

      dlclose(lib_handle);
    }
  else 
    {
//      std::cout<<std::endl<<"DEBUG >"<<"Nici un plug-in identificat pentru a executa commanda -> "<<decode_response_utf8(message);
//      std::cout<<std::endl<<"DEBUG >"<<"No plug-in available to execute command -> "<<decode_response_utf8(message);
    }
}



