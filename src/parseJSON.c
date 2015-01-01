#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <syslog.h>
#include <string.h>
#include "dds-globals.h"
#include "cJSON.h"
#include "dict.h"
#include "parseJSON.h"


SLIDE_ACTION parse_action(char *str) {
    
    if (!strcmp(str, "add-slide")) {
        return ADD_SLIDE;
    }
    else if (!strcmp(str, "delete-slide")) {
        return DELETE_SLIDE;
    }
    else if (!strcmp(str, "edit-slide")) {
        return EDIT_SLIDE;
    }
    else if (!strcmp(str, "Terminate")) {
        return TERMINATE;
    }
    else if(!strcmp(str, "load-slides")){
    	return LOAD_SLIDES;
    }
    else if(!strcmp(str, "connect")){
    	return CONNECT;
    }
    else {
        syslog(LOG_MAKEPRI(LOG_SYSLOG, LOG_ERR),
                "parse_action: Received an invalid slide action");
        closelog();
        return (SLIDE_ACTION) NULL;
    }
}

char *action_string(SLIDE_ACTION action) {
    if (action == ADD_SLIDE) {
        return "add-slide";
    }
    else if (action == DELETE_SLIDE) {
        return "delete-slide";
    }
    else if (action == EDIT_SLIDE) {
        return "edit-slide";
    }
    else if (action == TERMINATE) {
        return "Terminate";
    }
    else if (action == LOAD_SLIDES){
    	return "load-slides";
    }
    else if(action == CONNECT){
    	return "connect";
    }
    else {
        syslog(LOG_MAKEPRI(LOG_SYSLOG, LOG_ERR),
                "action_string: Recieved an invalid slide action");
        closelog();
        return NULL;
    }
}

pie *parse_pie(cJSON *raw_pie) {
	char *name = raw_pie->valuestring;
    pie *to_ret = malloc(sizeof(pie) + sizeof(name));
    to_ret->name = name;
    return to_ret;
}

// In case we need to do anything fancy down the road
char *pie_to_json(pie *parsed_pie) {
    return parsed_pie->name;
}

slide_action_info *make_slide_action_info(int id, char *location, int duration){
	slide_action_info *to_ret = malloc(sizeof(slide_action_info));
	to_ret->id = id;
	to_ret->location = location;
	to_ret->duration = duration;
	return to_ret;
}

action_data *parse_action_data(cJSON *raw_action_data){
	action_data *to_ret = malloc(sizeof(action_data));
	char *action_type = cJSON_GetObjectItem(raw_action_data, "type")->valuestring;
	if(!strcmp(action_type,"slide")){
		to_ret->type = ADT_SLIDE;
		to_ret->slide_data = make_slide_action_info(
				cJSON_GetObjectItem(raw_action_data, "ID")->valueint,
				cJSON_GetObjectItem(raw_action_data, "location")->valuestring,
				cJSON_GetObjectItem(raw_action_data, "duration")->valueint
		);
		return to_ret;
	}
	printf("parse_action_data: WARNING: Given unimplemented slide action. Returning Null Pointer.\n");
	return NULL;
}

cJSON *action_data_to_json(action_data *to_parse){
	cJSON *to_ret = cJSON_CreateObject();
	switch(to_parse->type){
	case ADT_SLIDE:
		cJSON_AddStringToObject(to_ret, "type", "slide");
		cJSON_AddNumberToObject(to_ret, "ID", to_parse->slide_data->id);
		cJSON_AddStringToObject(to_ret, "location", to_parse->slide_data->location);
		cJSON_AddNumberToObject(to_ret, "duration", to_parse->slide_data->duration);
		break;
	default:
		break;
	}
	return to_ret;
}

void parse_actions(cJSON *raw_actions, socket_message_content *receiver){
	int len = cJSON_GetArraySize(raw_actions);
	receiver->num_actions = len;
	receiver->actions = malloc(len * sizeof(action_data *));
	// Check if something went wrong
	if (receiver->actions == NULL) {
	    syslog(LOG_MAKEPRI(LOG_SYSLOG, LOG_ERR), "parse_actions: Could not allocate actions array for received message's content\n");
	    return;
	}
	int i;
	for(i = 0; i < len; i++){
		receiver->actions[i] = parse_action_data(cJSON_GetArrayItem(raw_actions, i));
	}
	return;
}

cJSON *actions_to_json(action_data *actions[], int num_actions){
	cJSON *to_ret = cJSON_CreateArray();
	int i;
	for(i = 0; i < num_actions; i ++){
		cJSON_AddItemToArray(to_ret, action_data_to_json(actions[i]));
	}
	return to_ret;
}


const char *parse_date(const char *input, struct tm *tm) {
    const char *cp;

    /* First clear the result structure.  */
    memset(tm, '\0', sizeof(*tm));

    /* Try the ISO format first.  */
    cp = strptime(input, "%FT%T%z", tm);
    if (cp == NULL) {
        /* Does not match.  Try the US form.  */
        cp = strptime(input, "%D", tm);
    }

    return cp;
}

void recursive_parse(cJSON *current, Dict *d, int arr_idx) {
    if (current == NULL) {return;}
    socket_meta *parsed = (socket_meta *)malloc(sizeof(socket_meta));
    if (current->child) {
        META_TYPE type;
        int nextnum;
        if (current->type == cJSON_Array) {
            parsed->type = T_ARR;
            nextnum = 0;
        }
        else if (current->type == cJSON_Object) {
            parsed->type = T_DICT;
            nextnum = -1;
        }
        Dict *ret_dict = make_dict();
        recursive_parse(current->child, ret_dict, nextnum);
        parsed->value = ret_dict;
    }
    else {
            if (current->type == cJSON_Number) {
            	if ((double)(current->valueint) == current->valuedouble){
            		int *val_int = malloc(sizeof(current->valueint));
                	*val_int = current->valueint;
                	parsed->type = T_INT;
                	parsed->value = val_int;
            	}
            	else{
            		float *val_doub = malloc(sizeof(current->valuedouble));
            		*val_doub = current->valuedouble;
            		parsed->type = T_DOUBLE;
            		parsed->value = val_doub;
            	}
            }
            else if (current->type == cJSON_String) {
                char *str;
                str = (char *)malloc(sizeof(char) * strlen(current->valuestring));
                strcpy(str, current->valuestring);
                parsed->type = T_POINT_CHAR;
                parsed->value = str;
            }
            else {
                void *value;
                if (current->valuestring) {
                    char *temp;
                    temp = (char *)malloc(sizeof(char) * strlen(current->valuestring));
                    strcpy(temp, current->valuestring);
                    value = (void *)temp;
                }
                else if (current->valueint) {
                    int *temp;
                    temp = (int *)malloc(sizeof(current->valueint));
                    *temp = current->valueint;
                    value = (void *)temp;
                }
                else if (current->valuedouble) {
                    double *temp;
                    temp = (double *)malloc(sizeof(current->valuedouble));
                    *temp = current->valuedouble;
                    value = (void *)temp;
                }
                parsed->type = T_POINT_VOID;
                parsed->value = value;
            }
    }
    if(current->string) {
        dict_put(d, current->string, (void *)parsed);
    }
    else{
    	char *arr_key = (char*)malloc(6 * sizeof(char)); // Limit array length to 1,000,000
    	sprintf(arr_key,"%d",arr_idx);
        dict_put(d, arr_key, (void *)parsed);
    }
    Dict dct;
    dct = *(d);
    while(dct.next){dct = *dct.next;}
    recursive_parse(current->next, d, ((arr_idx == -1) ? -1 : arr_idx + 1));
}

Dict *parse_json_meta(cJSON *raw_meta) {
    Dict *ret = make_dict();
    recursive_parse(raw_meta->child, ret, -1);
    return ret;
}

void parse_and_add_to(Dict *to_add, cJSON *add_to, int is_object) {
	if (to_add){
		if (to_add->value){
        if ((((socket_meta *) to_add->value)->type) == T_INT){
            if (is_object) {
                cJSON_AddNumberToObject(add_to, to_add->key, *((int*)((socket_meta *) to_add->value)->value));
            }
            else {
                cJSON_AddNumberToArray(add_to, *((int*)((socket_meta *) to_add->value)->value));
            }
        }
        else if ((((socket_meta *) to_add->value)->type) == T_DOUBLE){
            if (is_object) {
                cJSON_AddNumberToObject(add_to, to_add->key, *((double*)((socket_meta *) to_add->value)->value));
            }
            else {
                cJSON_AddNumberToArray(add_to, *((double*)((socket_meta *) to_add->value)->value));
            }
        }
        else if ((((socket_meta *) to_add->value)->type) == T_POINT_CHAR) {
            if (is_object) {
                cJSON_AddStringToObject(add_to, to_add->key, (char *) ((socket_meta *) to_add->value)->value);
            }
            else {
                cJSON_AddStringToArray(add_to, (char *) ((socket_meta *) to_add->value)->value);
            }
        }
        else if ((((socket_meta *) to_add->value)->type) == T_ARR) {
            cJSON *temp = cJSON_CreateArray();
            parse_and_add_to((Dict *) ((socket_meta *) to_add->value)->value, temp, 0);
            if (is_object) {
                cJSON_AddItemReferenceToObject(add_to, to_add->key, temp);
            }
            else {
                cJSON_AddItemReferenceToArray(add_to, temp);
            }
        }
        else if ((((socket_meta *) to_add->value)->type) == T_DICT) {
            cJSON *temp = cJSON_CreateObject();
            parse_and_add_to((Dict *) ((socket_meta *) to_add->value)->value, temp, 1);
            if (is_object) {
                cJSON_AddItemReferenceToObject(add_to, to_add->key, temp);
            }
            else {
                cJSON_AddItemReferenceToArray(add_to, temp);
            }
        }
    parse_and_add_to(to_add->next, add_to, is_object);
		}
		else if(to_add->next){
			parse_and_add_to(to_add->next,add_to,is_object);
		}
	}
}

cJSON *parse_message_meta(Dict *msg_meta) {
    cJSON *meta;
    meta = cJSON_CreateObject();
    parse_and_add_to((Dict *) msg_meta->next, meta, 1); //next b/c first dict entry is null
    return meta;
}

socket_message *json_to_message(char *str) {
    cJSON *input, *content;
    input = cJSON_Parse(str);
    content = cJSON_GetObjectItem(input, "content");
    if(content->type == cJSON_String){
    	content = cJSON_Parse(content->valuestring);
    }
    socket_message_content *msg_c = (socket_message_content *) malloc(sizeof(socket_message_content));
    parse_actions(cJSON_GetObjectItem(content,"actions"),msg_c);
    cJSON *mt;
    if(mt = cJSON_GetObjectItem(content, "meta")){
    	msg_c->meta = parse_json_meta(mt);
    }
    socket_message *msg = (socket_message *) malloc(sizeof(socket_message));
    msg->datetime = malloc(sizeof(struct tm));
    if(cJSON_GetObjectItem(input, "datetime")->type != cJSON_NULL){
    	parse_date(cJSON_GetObjectItem(input, "datetime")->valuestring, msg->datetime);
    }else{
	free(msg->datetime);
    	msg->datetime = NULL;
    }
    msg->action = parse_action(cJSON_GetObjectItem(input, "action")->valuestring);
    msg->content = msg_c;
    msg->src = parse_pie(cJSON_GetObjectItem(input, "src"));
    msg->dest = parse_pie(cJSON_GetObjectItem(input, "dest"));
    msg->plugin_dest = cJSON_GetObjectItem(input, "pluginDest")->valuestring;
    return msg;
}

char *message_to_json(socket_message *msg) {

    cJSON *root, *content;
    root = cJSON_CreateObject();
    char datetime[27];
    strftime(datetime, sizeof(datetime), "%FT%T%z", msg->datetime);
    cJSON_AddStringToObject(root, "datetime", datetime);
    cJSON_AddStringToObject(root, "action", action_string(msg->action));
    cJSON_AddStringToObject(root, "src", pie_to_json(msg->src));
    cJSON_AddStringToObject(root, "dest", pie_to_json(msg->dest));
    if(msg->plugin_dest){
    	cJSON_AddStringToObject(root, "pluginDest", msg->plugin_dest);
    }
    cJSON_AddItemToObject(root, "content", content = cJSON_CreateObject());
    cJSON_AddItemToObject(content, "actions", actions_to_json(msg->content->actions, msg->content->num_actions));
    if(msg->content->meta){
    	cJSON_AddItemToObject(content, "meta", parse_message_meta(msg->content->meta));
    }
    return cJSON_Print(root);
}

void dump_meta_atom(socket_meta *mem, int indents);
void dump_meta_dict(Dict *d, int indents);

void dump_meta_dict(Dict *d, int indents){
	int i = 0;
	char tabs[indents + 1];
	for(;i<indents;i++){
		tabs[i] = '\t';
	}
	tabs[indents] = '\0';
	int len = dict_size(d);
	i = 0;
	Dict *temp = d;
	for(;i<=len;i++){
		printf("%s%s\t: ",tabs,temp->key);
		dump_meta_atom((socket_meta *)temp->value, indents);
		printf((i < len-1) ? ",\n" : "\n");
		temp = temp->next;
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
void dump_meta_atom(socket_meta *mem, int indents){
	int i = 0;
	char tabs[indents + 1];
	for(;i<indents;i++){
		tabs[i] = '\t';
	}
	tabs[indents] = '\0';
	if(mem->type == T_INT){
		printf("%d",*(int *)mem->value);
	}
	else if(mem->type == T_DOUBLE){
		printf("%f",*(double *)mem->value);
	}
	else if(mem->type == T_CHAR){
		printf("%c",*(char *)mem->value);
	}
	else if(mem->type == T_POINT_INT){
		printf("%d (0x%x)", *(int *)mem->value, (int)mem->value);
	}
	else if(mem->type == T_POINT_CHAR){
		printf("%s (0x%x)", (char *)mem->value, (int)mem->value);
	}
	else if(mem->type == T_POINT_VOID){
		printf("<void pointer>");
	}
	else if(mem->type == T_ARR){
		printf("Array: [\n");
		dump_meta_dict(((Dict*)mem->value)->next, indents + 1);
		printf("%s\t\t]",tabs);
	}
	else if(mem->type == T_DICT){
		printf("Object: {\n");
		dump_meta_dict(((Dict*)mem->value)->next, indents + 1);
		printf("%s\t\t}",tabs);
	}
}
#pragma GCC diagnostic pop

void dump_meta(Dict *meta){
	printf("meta: {\n");
	dump_meta_dict(meta->next, 1);
	printf("}\n");
}

Dict *meta_to_dict(Dict *meta){
	if (meta == NULL){return NULL;}
	Dict *to_ret = malloc(sizeof(*meta));
	memcpy(to_ret,meta,sizeof(*meta));
	if(to_ret->value != NULL){
		if ((((socket_meta*)to_ret->value)->type == T_ARR) || (((socket_meta*)to_ret->value)->type == T_DICT)){
			to_ret->value = meta_to_dict((Dict *)((socket_meta*)meta->value)->value);
		}
		else{
			to_ret->value = ((socket_meta*)to_ret->value)->value;
		}
	}
	to_ret->next = meta_to_dict(meta->next);
	return to_ret;
}

/*
//Testing function...compile with gcc parseJSON.c cJSON.c dict.c -lm -lrt
//char json_string[] = "{\"datetime\" : \"2014-11-30T22:04:15+0000\",\"action\" : \"add-slide\",\"pies\" : [{ \"name\" : \"shepard\" },{ \"name\" : \"blueberry\" }],\"content\"  : {\"ID\" : 14,\"Permalink\" : \"http://dds-wp...\", \"meta\" : {\"key1\" : [ \"value\" ],\"key2\" : [\"value1\",\"value2\",3,{\"meta can be weird\" : \"remember that\"}]}}}";
//char json_string[] = "{\"src\" : \"WPHandler\", \"dest\" : \"keylime\", \"datetime\" : \"2014-11-30T22:04:15+0000\", \"content\" : {\"actions\" : [{\"ID\" : 226,\"type\" : \"slide\",\"location\" : \"http:\\/\\/www.ccs.neu.edu\\/systems\\/labstats\\/212.html\",\"duration\" : 1},{\"ID\" : 194,\"type\" : \"slide\",\"location\" : \"http:\\/\\/10.0.0.202\\/weather2\\/\",\"duration\" : 15}]}, \"pluginDest\" : \"slideShow\", \"action\" : \"load-slides\"}";
//char json_string[] = "{\"src\" : \"WPHandler\", \"dest\" : \"keylime\", \"datetime\" : \"2014-11-30T22:04:15+0000\", \"content\" : {\"actions\" : [{\"ID\" : 226,\"type\" : \"slide\",\"location\" : \"http:\\/\\/www.ccs.neu.edu\\/systems\\/labstats\\/212.html\",\"duration\" : 1},{\"ID\" : 194,\"type\" : \"slide\",\"location\" : \"http:\\/\\/10.0.0.202\\/weather2\\/\",\"duration\" : 15}], \"meta\" : {\"key1\" : [ \"value\" ],\"key2\" : [\"value1\",\"value2\",3,{\"meta can be weird\" : \"remember that\"}]}}, \"pluginDest\" : \"slideShow\", \"action\" : \"load-slides\"}";
char json_string1[] = "{\"src\": \"WPHandler\", \"dest\": \"keylime\", \"datetime\": \"2014-11-30T22:04:15+0000\", \"content\": {\"actions\":[{\"type\":\"slide\",\"ID\": 12, \"location\":\"https:\\/\\/twitter.com\\/swiftonsecurity\",\"duration\":20},{\"type\":\"slide\",\"ID\": 27, \"location\":\"http:\\/\\/dds-wp.ccs.neu.edu\\/?slide=t-rex-trying&pie_name=keylime\",\"duration\":5},{\"type\":\"slide\",\"ID\": 55, \"location\":\"http:\\/\\/www.ccs.neu.edu\\/systems\\/labstats\\/212.html\",\"duration\":1},{\"type\":\"slide\",\"ID\": 94, \"location\":\"http:\\/\\/radar.weather.gov\\/ridge\\/Conus\\/Loop\\/NatLoop.gif\",\"duration\":20}]}, \"pluginDest\": \"slideShow\", \"action\": \"load-slides\"}";

char json_string[] = "{\"src\": \"WPHandler\", \"dest\": \"keylime\", \"datetime\": \"2014-12-30T21:53:45+0000\", \"content\": {\"actions\":[{\"ID\":5,\"type\":\"slide\",\"location\":\"http:\\/\\/192.168.11.132\\/?slide=test1&pie_name=keylime\",\"duration\":1}]}, \"pluginDest\": \"slideShow\", \"action\": \"load-slides\"}";
int main() {
    socket_message *sm = json_to_message(json_string);
    #define DT_BUF_SIZE 27
    char dt[ DT_BUF_SIZE ];
    strftime(dt, sizeof(dt), "%Y-%m-%dT%H:%M:%S%z", sm->datetime);
    printf("sm:\ndatetime: %s\naction: %s\nsrc: %s\ndest: %s\ncontent:\n\t\n...\n",
            dt, action_string(sm->action), sm->src->name, sm->dest->name);
    if(sm->content->meta){
    	dump_meta(sm->content->meta);
    	printf("sm->content->meta->key2->2: %d\n", *(int*)dict_get_val(meta_to_dict(sm->content->meta),"key2","2"));
    }
    printf("back again:\n%s\n", message_to_json(sm));
}
*/
#ifdef ___TEST_SUITES___

/*
 * TEST SUITES
 */

void TestParseAction(CuTest *tc){
	CuAssertTrue(tc,parse_action("add-slide") == ADD_SLIDE);
	CuAssertTrue(tc,parse_action("delete-slide") == DELETE_SLIDE);
	CuAssertTrue(tc,parse_action("edit-slide") == EDIT_SLIDE);
	CuAssertTrue(tc,parse_action("Terminate") == TERMINATE);
	CuAssertTrue(tc,parse_action("load-slides") == LOAD_SLIDES);
	CuAssertTrue(tc,parse_action("connect") == CONNECT);
}

void TestActionString(CuTest *tc){
	CuAssertStrEquals(tc,"add-slide",action_string(ADD_SLIDE));
	CuAssertStrEquals(tc,"delete-slide",action_string(DELETE_SLIDE));
	CuAssertStrEquals(tc,"edit-slide",action_string(EDIT_SLIDE));
	CuAssertStrEquals(tc,"Terminate",action_string(TERMINATE));
	CuAssertStrEquals(tc,"load-slides",action_string(LOAD_SLIDES));
	CuAssertStrEquals(tc,"connect",action_string(CONNECT));
}

void TestParsePie(CuTest *tc){
	cJSON *test_cjson1 = cJSON_CreateString("keylime");
	cJSON *test_cjson2 = cJSON_CreateString("chocolate");
	pie *p1 = parse_pie(test_cjson1);
	pie *p2 = parse_pie(test_cjson2);
	CuAssertStrEquals(tc,p1->name,"keylime");
	CuAssertStrEquals(tc,p2->name,"chocolate");
	cJSON_Delete(test_cjson1);
	cJSON_Delete(test_cjson2);
	free(p1);
	free(p2);
}

void TestPieToJson(CuTest *tc){
	pie p1 = (pie){"keylime"};
	pie p2 = (pie){"chocolate"};
	char *test_json1 = pie_to_json(&p1);
	char *test_json2 = pie_to_json(&p2);
	CuAssertStrEquals(tc,test_json1,"keylime");
	CuAssertStrEquals(tc,test_json2,"chocolate");
}

void TestMakeSlideActionInfo(CuTest *tc){
	slide_action_info *test = make_slide_action_info(193,"localhost",27);
	CuAssertIntEquals(tc,test->id,193);
	CuAssertStrEquals(tc,test->location,"localhost");
	CuAssertIntEquals(tc,test->duration,27);
	free(test);
}

void TestParseActionData(CuTest *tc){
	cJSON *test1 = cJSON_CreateObject();
	cJSON *test2 = cJSON_CreateObject();
	cJSON_AddStringToObject(test1,"type","slide");
	cJSON_AddStringToObject(test2,"type","attempted-stack-smash");
	cJSON_AddNumberToObject(test1,"ID",193);
	cJSON_AddStringToObject(test1,"location","localhost");
	cJSON_AddNumberToObject(test1,"duration",27);
	action_data to_comp = (action_data){ADT_SLIDE,make_slide_action_info(193,"localhost",27)};
	action_data *testp1 = parse_action_data(test1);
	action_data *testp2 = parse_action_data(test2);
	CuAssertTrue(tc,((testp1->type == ADT_SLIDE) && (to_comp.type == ADT_SLIDE)));
	CuAssertIntEquals(tc,testp1->slide_data->id,to_comp.slide_data->id);
	CuAssertStrEquals(tc,testp1->slide_data->location,to_comp.slide_data->location);
	CuAssertIntEquals(tc,testp1->slide_data->duration,to_comp.slide_data->duration);
	CuAssertPtrEquals(tc,testp2,NULL);
	cJSON_Delete(test1);
	cJSON_Delete(test2);
	free(testp1);
	free(testp2);
}

CuSuite *ParseJSONGetSuite(void){
	CuSuite *suite = CuSuiteNew();
	SUITE_ADD_TEST(suite,TestParseAction);
	SUITE_ADD_TEST(suite,TestActionString);
	SUITE_ADD_TEST(suite,TestParsePie);
	SUITE_ADD_TEST(suite,TestPieToJson);
	SUITE_ADD_TEST(suite,TestMakeSlideActionInfo);
	return suite;
}

#endif
