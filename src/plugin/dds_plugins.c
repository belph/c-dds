#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include "dds_globals.h"
#include "dict.h"
#include "dds_plugins.h"




// ---------------
static void stackDump (lua_State *L) {
      int i;
      int top = lua_gettop(L);
      printf("Stack size %d\n", top);
      for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
    
          case LUA_TSTRING:  /* strings */
            printf("`%s'", lua_tostring(L, i));
            break;
    
          case LUA_TBOOLEAN:  /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
    
          case LUA_TNUMBER:  /* numbers */
            printf("%g", lua_tonumber(L, i));
            break;
    
          default:  /* other values */
            printf("%s", lua_typename(L, t));
            break;
    
        }
        printf("  ");  /* put a separator */
      }
      printf("\n");  /* end the listing */
    }

static int well_formed(char *list, int *count){
	
	#define outside(var, s) do{var = (var && s[0]!=',' && s[strlen(s)-1]!=',');}while(0);
	int good = 1;
	//outside(good, list)
	char* cur_split = strtok(list, ",");
	while(cur_split){
		//outside(good, cur_split)
		printf("Got %s\n", cur_split);
		(*count)++;
		printf("Count = %d\n", *count);
		cur_split = strtok(NULL, ",");
	}
	#undef outside
	printf("Made it!\n");
	return good;

}
static char* path_to_plugin(const char *name){
	int char_count = strlen(PLUG_FOLDR) + strlen(name) + 1;
	char* str = malloc(char_count);
	*str = '\0';
	strcat(str, PLUG_FOLDR);
	strcat(str, name);
	return str;
}
static int init_plugin(char *name, lua_plugin *target, char* plugin_file){
	target->instance = luaL_newstate();
	luaL_openlibs(target->instance);
	printf("Attempting to load %s\n", plugin_file);
	TEST(!luaL_loadfile(target->instance, plugin_file), "Couldn't load file for a plugin...");
	TEST(!lua_pcall(target->instance, 0, 0, 0), "Couldn't run plugin file...");
	printf("globals ... \n");
	lua_getglobal(target->instance, "name");
	printf("Stackdump : \n");
	stackDump(target->instance);
	printf("Attempting to print...\n");
	TEST(!lua_pcall(target->instance, 0, 1, 0), "Couldn't call name...");
	stackDump(target->instance);
	ASSERT(lua_isstring(target->instance, -1), "It wasn't a string...")
	printf("Got %s from plugin\n", lua_tostring(target->instance, -1));
	printf("Done printing...\n");
	return 1;
		
}
char* get_plugins_list_str(Dict* d){
	if(!d)
		return NULL;
	d = d->next;
	while(d){
		printf("Key %s\n", d->key);
		if(!strcmp(d->key, "plugins")){
			printf("Returning val...\n");
			return (char*)d->value;
		}
		d = d->next;
	}
	printf("No key ...\n");
	return NULL;

}
lua_plugin_list *init_plugins(Dict *config){
	printf("Initializing plugins...\n");
	static char once = 0;
	ASSERT(once++ == 0, "init_plugins was run more then once...") //We only run this once
	printf("After assert\n");
	lua_plugin_list plugins;
	printf("After plugin list...\n");
	dump_dict(config);
	printf("After val\n");
	char* plugins_list_str = get_plugins_list_str(config);
	printf("Got %s as list\n", plugins_list_str);

	int plugin_count = 0;
	if(!well_formed(plugins_list_str, &plugin_count)){
		printf("Inside wellformed...\n");
		PERR("Bad plugin list in config file...")
	}
	printf("Blah1\n");	
	plugins.num_plugins = plugin_count;
	plugins.list = malloc(sizeof(lua_plugin) * plugin_count);
	printf("Blah2\n");
	lua_plugin *iter = plugins.list;


	char* cur_plugin;
	//TODO count tokens...
	cur_plugin = strtok(plugins_list_str, ",");
	while(cur_plugin){
		printf("While\n");
		char* file_name = path_to_plugin(cur_plugin);	
		printf("Got filename %s\n", file_name);
		TEST(init_plugin(cur_plugin, iter, file_name), "Failed to initialize plugin");
		free(file_name);

		cur_plugin = strtok(NULL, ",");
		iter++;
	}
	printf("Done loading plugins...\n");
	return NULL;
}

