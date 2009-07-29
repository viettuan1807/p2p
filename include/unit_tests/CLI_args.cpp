//include
#include <CLI_args.hpp>
#include <logger.hpp>

//standard

int main()
{
	char * argv[3];
	argv[0] = static_cast<char *>(std::malloc(10));
	argv[1] = static_cast<char *>(std::malloc(10));
	argv[2] = static_cast<char *>(std::malloc(10));
	argv[0] = "prog_name";
	argv[1] = "-b";
	argv[2] = "--x=123";

	CLI_args CLI_Args(3, argv);

	//check_bool()
	if(!CLI_Args.check_bool("-b")){
		LOGGER; exit(1);
	}

	//get_string()
	std::string str;
	if(!CLI_Args.get_string("--x", str)){
		LOGGER; exit(1);
	}
	if(str != "123"){
		LOGGER; exit(1);
	}

	//get_uint()
	unsigned uint;
	if(!CLI_Args.get_uint("--x", uint)){
		LOGGER; exit(1);
	}
	if(uint != 123){
		LOGGER; exit(1);
	}
}