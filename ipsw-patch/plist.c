#include <stdlib.h>
#include <string.h>
#include <xpwn/plist.h>
#include "common.h"

Tag* getNextTag(char** xmlPtr) {
	char* xml;
	char* tagEnd;
	char* curChar;
	Tag* tag;
	int tagDepth;
	
	xml = *xmlPtr;
	xml = strchr(xml, '<');
	
	if(xml == NULL) {
		return NULL;
	}
	
	tag = (Tag*) malloc(sizeof(Tag));	

	tagEnd = strchr(xml, '>');
	tag->name = (char*) malloc(sizeof(char) * (tagEnd -  xml));
	memcpy(tag->name, xml + 1, tagEnd -  xml - 1);
	tag->name[tagEnd -  xml - 1] = '\0';
	if((tagEnd -  xml - 1) > 0) {
		if(tag->name[tagEnd -  xml - 2] == '/') {
			tag->xml = malloc(sizeof(char) * 1);
			tag->xml[0] = '\0';
			*xmlPtr = tagEnd + 1;
			return tag;
		}
	}
	
	xml = tagEnd;
	curChar = xml;
	tagDepth = 1;
	while(*curChar != '\0') {
		if(*curChar == '<') {
			if(*(curChar + 1) == '/') {
				tagDepth--;
				if(tagDepth == 0) {
					break;
				}
			} else {
				tagDepth++;
			}
		} else if(*curChar == '>') {
			if(*(curChar - 1) == '/') {
				tagDepth--;
			}
		}
		curChar++;
	}
	
	tag->xml = (char*) malloc(sizeof(char) * (curChar -  xml));
	memcpy(tag->xml, xml + 1, curChar -  xml - 1);
	tag->xml[curChar -  xml - 1] = '\0';
	
	*xmlPtr = strchr(curChar, '>') + 1;
	
	return tag;
}

void releaseTag(Tag* tag) {
	free(tag->name);
	free(tag->xml);
	free(tag);
}

void releaseArray(ArrayValue* myself) {
	int i;
	
	free(myself->dValue.key);
	for(i = 0; i < myself->size; i++) {
		switch(myself->values[i]->type) {
			case DictionaryType:
				releaseDictionary((Dictionary*) myself->values[i]);
				break;
			case ArrayType:
				releaseArray((ArrayValue*) myself->values[i]);
				break;
			case StringType:
				free(((StringValue*)(myself->values[i]))->value);
			case BoolType:
			case IntegerType:
				free(myself->values[i]->key);
				free(myself->values[i]);
				break;
		}
	}
	free(myself->values);
	free(myself);
}

void releaseDictionary(Dictionary* myself) {
	DictValue* next;
	DictValue* toRelease;
	
	free(myself->dValue.key);
	next = myself->values;
	while(next != NULL) {
		toRelease = next;
		next = next->next;
		
		switch(toRelease->type) {
			case DictionaryType:
				releaseDictionary((Dictionary*) toRelease);
				break;
			case ArrayType:
				releaseArray((ArrayValue*) toRelease);
				break;
			case StringType:
				free(((StringValue*)(toRelease))->value);
			case IntegerType:
			case BoolType:
				free(toRelease->key);
				free(toRelease);
				break;
		}
	}
	free(myself);
}
void createArray(ArrayValue* myself, char* xml) {
	Tag* valueTag;
	DictValue* curValue;
	
	myself->values = NULL;
	myself->size = 0;
	while(*xml != '\0') {
		valueTag = getNextTag(&xml);
		if(valueTag == NULL)  {
			break;
		}
		
		myself->size++;
		myself->values = realloc(myself->values, sizeof(DictValue*) * myself->size);
		curValue = (DictValue*) malloc(sizeof(DictValue));

		curValue->key = (char*) malloc(sizeof("arraykey"));
		strcpy(curValue->key, "arraykey");
		curValue->next = NULL;

		if(strcmp(valueTag->name, "dict") == 0) {
			curValue->type = DictionaryType;
			curValue = (DictValue*) realloc(curValue, sizeof(Dictionary));
			createDictionary((Dictionary*) curValue, valueTag->xml);
		} else if(strcmp(valueTag->name, "string") == 0) {
			curValue->type = StringType;
			curValue = (DictValue*) realloc(curValue, sizeof(StringValue));
			((StringValue*)curValue)->value = (char*) malloc(sizeof(char) * (strlen(valueTag->xml) + 1));
			strcpy(((StringValue*)curValue)->value, valueTag->xml);
		} else if(strcmp(valueTag->name, "data") == 0) {
			size_t len;
			curValue->type = StringType;
			curValue = (DictValue*) realloc(curValue, sizeof(DataValue));
			((DataValue*)curValue)->value = decodeBase64(valueTag->xml, &len);
			((DataValue*)curValue)->len = len;
		} else if(strcmp(valueTag->name, "integer") == 0) {
			curValue->type = IntegerType;
			curValue = (DictValue*) realloc(curValue, sizeof(IntegerValue));
			sscanf(valueTag->xml, "%d", &(((IntegerValue*)curValue)->value));
		} else if(strcmp(valueTag->name, "array") == 0) {
			curValue->type = ArrayType;
			curValue = (DictValue*) realloc(curValue, sizeof(ArrayValue));
			createArray((ArrayValue*) curValue, valueTag->xml);
		} else if(strcmp(valueTag->name, "true/") == 0) {
			curValue->type = BoolType;
			curValue = (DictValue*) realloc(curValue, sizeof(BoolValue));
			((BoolValue*)curValue)->value = TRUE;
		} else if(strcmp(valueTag->name, "false/") == 0) {
			curValue->type = BoolType;
			curValue = (DictValue*) realloc(curValue, sizeof(BoolValue));
			((BoolValue*)curValue)->value = FALSE;
		}
		
		myself->values[myself->size - 1] = curValue;
		
		releaseTag(valueTag);
	}
}

void removeKey(Dictionary* dict, char* key) {
	DictValue* next;
	DictValue* toRelease;
	next = dict->values;
	while(next != NULL) {
		if(strcmp(next->key, key) == 0) {
			toRelease = next;
			if(toRelease->prev) {
				toRelease->prev->next = toRelease->next;
			} else {
				dict->values = toRelease->next;
			}
			if(toRelease->next) {
				toRelease->next->prev = toRelease->prev;
			}
			switch(toRelease->type) {
				case DictionaryType:
					releaseDictionary((Dictionary*) toRelease);
					break;
				case ArrayType:
					releaseArray((ArrayValue*) toRelease);
					break;
				case StringType:
					free(((StringValue*)(toRelease))->value);
					free(toRelease->key);
					free(toRelease);
					break;
				case DataType:
					free(((DataValue*)(toRelease))->value);
					free(toRelease->key);
					free(toRelease);
					break;	
				case IntegerType:
				case BoolType:
					free(toRelease->key);
					free(toRelease);
					break;
			}
			return;
		}
		next = next->next;
	}

	return;
}

void createDictionary(Dictionary* myself, char* xml) {
	Tag* keyTag;
	Tag* valueTag;
	DictValue* curValue;
	DictValue* lastValue;
	
	curValue = NULL;
	lastValue = NULL;
	
	while(*xml != '\0') {
		keyTag = getNextTag(&xml);
		if(keyTag == NULL)  {
			break;
		}
		
		if(strcmp(keyTag->name, "key") != 0) {
			releaseTag(keyTag);
			continue;
		}
		
		curValue = (DictValue*) malloc(sizeof(DictValue));
		curValue->key = (char*) malloc(sizeof(char) * (strlen(keyTag->xml) + 1));
		strcpy(curValue->key, keyTag->xml);
		curValue->next = NULL;
		releaseTag(keyTag);
		
		
		valueTag = getNextTag(&xml);
		if(valueTag == NULL)  {
			break;
		}
		
		if(strcmp(valueTag->name, "dict") == 0) {
			curValue->type = DictionaryType;
			curValue = (DictValue*) realloc(curValue, sizeof(Dictionary));
			createDictionary((Dictionary*) curValue, valueTag->xml);
		} else if(strcmp(valueTag->name, "string") == 0) {
			curValue->type = StringType;
			curValue = (DictValue*) realloc(curValue, sizeof(StringValue));
			((StringValue*)curValue)->value = (char*) malloc(sizeof(char) * (strlen(valueTag->xml) + 1));
			strcpy(((StringValue*)curValue)->value, valueTag->xml);
		} else if(strcmp(valueTag->name, "data") == 0) {
			size_t len;
			curValue->type = StringType;
			curValue = (DictValue*) realloc(curValue, sizeof(DataValue));
			((DataValue*)curValue)->value = decodeBase64(valueTag->xml, &len);
			((DataValue*)curValue)->len = len;
		} else if(strcmp(valueTag->name, "integer") == 0) {
			curValue->type = IntegerType;
			curValue = (DictValue*) realloc(curValue, sizeof(IntegerValue));
			sscanf(valueTag->xml, "%d", &(((IntegerValue*)curValue)->value));
		} else if(strcmp(valueTag->name, "array") == 0) {
			curValue->type = ArrayType;
			curValue = (DictValue*) realloc(curValue, sizeof(ArrayValue));
			createArray((ArrayValue*) curValue, valueTag->xml);
		} else if(strcmp(valueTag->name, "true/") == 0) {
			curValue->type = BoolType;
			curValue = (DictValue*) realloc(curValue, sizeof(BoolValue));
			((BoolValue*)curValue)->value = TRUE;
		} else if(strcmp(valueTag->name, "false/") == 0) {
			curValue->type = BoolType;
			curValue = (DictValue*) realloc(curValue, sizeof(BoolValue));
			((BoolValue*)curValue)->value = FALSE;
		}

		curValue->prev = lastValue;
		if(lastValue == NULL) {
			myself->values = curValue;
		} else {
			lastValue->next = curValue;
		}
		
		lastValue = curValue;
		
		releaseTag(valueTag);
	}
}

char* getXmlFromArrayValue(ArrayValue* myself, int tabsCount) {
	char tabs[100];
	char buffer[4096];
	char* toReturn;
	char* ret;
	size_t toReturnSize;
	int i;
	DictValue* curValue;
	
	tabs[0] = '\0';
	for(i = 0; i < tabsCount; i++) {
		strcat(tabs, "\t");
	}

	
	sprintf(buffer, "%s<array>\n", tabs);
	toReturnSize = sizeof(char) * (strlen(buffer) + 1);
	toReturn = malloc(toReturnSize);
	toReturn = strcpy(toReturn, buffer);
	
	for(i = 0; i < myself->size; i++) {
		curValue = myself->values[i];

		if(curValue->type == DictionaryType) {
			ret = getXmlFromDictionary((Dictionary*)curValue, tabsCount + 1);
			toReturnSize += sizeof(char) * (strlen(ret) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, ret);
			free(ret);
		} else if(curValue->type == StringType) {
			sprintf(buffer, "%s\t<string>%s</string>\n", tabs, ((StringValue*)curValue)->value);
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
		} else if(curValue->type == DataType) {
			char* base64 = convertBase64(((DataValue*)curValue)->value, ((DataValue*)curValue)->len, 0, -1);
			sprintf(buffer, "%s\t<data>", tabs);
			toReturnSize += sizeof(char) * (strlen(buffer) + strlen(base64) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
			toReturn = strcat(toReturn, base64);
			sprintf(buffer, "</data>\n", tabs);
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
			free(base64);
		} else if(curValue->type == IntegerType) {
			sprintf(buffer, "%s\t<integer>%d</integer>\n", tabs, ((IntegerValue*)curValue)->value);
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
		} else if(curValue->type == ArrayType) {
			ret = getXmlFromArrayValue((ArrayValue*)curValue, tabsCount + 1);
			toReturnSize += sizeof(char) * (strlen(ret) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, ret);
			free(ret);
		} else if(curValue->type == BoolType) {
			if(((BoolValue*)curValue)->value) {
				sprintf(buffer, "%s\t<true/>\n", tabs);
			} else {
				sprintf(buffer, "%s\t<false/>\n", tabs);
			}
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
		}
	}
	
	sprintf(buffer, "%s</array>\n", tabs);
	toReturnSize += sizeof(char) * (strlen(buffer) + 1);
	toReturn = realloc(toReturn, toReturnSize);
	toReturn = strcat(toReturn, buffer);
	
	return toReturn;
}

char* getXmlFromDictionary(Dictionary* myself, int tabsCount) {
	char tabs[100];
	char buffer[4096];
	char* toReturn;
	char* ret;
	size_t toReturnSize;
	int i;
	DictValue* curValue;
	
	tabs[0] = '\0';
	for(i = 0; i < tabsCount; i++) {
		strcat(tabs, "\t");
	}

	
	sprintf(buffer, "%s<dict>\n", tabs);
	toReturnSize = sizeof(char) * (strlen(buffer) + 1);
	toReturn = malloc(toReturnSize);
	toReturn = strcpy(toReturn, buffer);
	
	curValue = myself->values;
	while(curValue != NULL) {
		sprintf(buffer, "%s\t<key>%s</key>\n", tabs, curValue->key);
		toReturnSize += sizeof(char) * (strlen(buffer) + 1);
		toReturn = realloc(toReturn, toReturnSize);
		toReturn = strcat(toReturn, buffer);
		
		if(curValue->type == DictionaryType) {
			ret = getXmlFromDictionary((Dictionary*)curValue, tabsCount + 1);
			toReturnSize += sizeof(char) * (strlen(ret) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, ret);
			free(ret);
		} else if(curValue->type == StringType) {
			sprintf(buffer, "%s\t<string>%s</string>\n", tabs, ((StringValue*)curValue)->value);
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
		} else if(curValue->type == DataType) {
			char* base64 = convertBase64(((DataValue*)curValue)->value, ((DataValue*)curValue)->len, 0, -1);
			sprintf(buffer, "%s\t<data>", tabs);
			toReturnSize += sizeof(char) * (strlen(buffer) + strlen(base64) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
			toReturn = strcat(toReturn, base64);
			sprintf(buffer, "</data>\n", tabs);
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
			free(base64);
		} else if(curValue->type == IntegerType) {
			sprintf(buffer, "%s\t<integer>%d</integer>\n", tabs, ((IntegerValue*)curValue)->value);
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
		} else if(curValue->type == ArrayType) {
			ret = getXmlFromArrayValue((ArrayValue*)curValue, tabsCount + 1);
			toReturnSize += sizeof(char) * (strlen(ret) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, ret);
			free(ret);
		} else if(curValue->type == BoolType) {
			if(((BoolValue*)curValue)->value) {
				sprintf(buffer, "%s\t<true/>\n", tabs);
			} else {
				sprintf(buffer, "%s\t<false/>\n", tabs);
			}
			toReturnSize += sizeof(char) * (strlen(buffer) + 1);
			toReturn = realloc(toReturn, toReturnSize);
			toReturn = strcat(toReturn, buffer);
		}
		
		curValue = curValue->next;
	}
	
	sprintf(buffer, "%s</dict>\n", tabs);
	toReturnSize += sizeof(char) * (strlen(buffer) + 1);
	toReturn = realloc(toReturn, toReturnSize);
	toReturn = strcat(toReturn, buffer);
	
	return toReturn;
}

Dictionary* createRoot(char* xml) {
	Tag* tag;
	Dictionary* dict;
	
	xml = strstr(xml, "<dict>");
	tag = getNextTag(&xml);
	dict = malloc(sizeof(Dictionary));
	dict->dValue.next = NULL;
	dict->dValue.key = malloc(sizeof("root"));
	strcpy(dict->dValue.key, "root");
	dict->values = NULL;
	createDictionary(dict, tag->xml);
	releaseTag(tag);
	return dict;
}

char* getXmlFromRoot(Dictionary* root) {
	char buffer[4096];
	char* toReturn;
	char* ret;
	size_t toReturnSize;
	
	sprintf(buffer, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n");
	toReturnSize = sizeof(char) * (strlen(buffer) + 1);
	toReturn = malloc(toReturnSize);
	toReturn = strcpy(toReturn, buffer);
	
	ret = getXmlFromDictionary(root, 0);
	toReturnSize += sizeof(char) * (strlen(ret) + 1);
	toReturn = realloc(toReturn, toReturnSize);
	toReturn = strcat(toReturn, ret);
	free(ret);
	
	sprintf(buffer, "</plist>\n");
	toReturnSize += sizeof(char) * (strlen(buffer) + 1);
	toReturn = realloc(toReturn, toReturnSize);
	toReturn = strcat(toReturn, buffer);
	
	return toReturn;

}

DictValue* getValueByKey(Dictionary* myself, const char* key) {
	DictValue* next;

	if(myself == NULL) {
		return NULL;
	}

	next = myself->values;
	while(next != NULL) {
		if(strcmp(next->key, key) == 0)
			return next;
		next = next->next;
	}
	return NULL;
}

void addStringToArray(ArrayValue* array, char* str) {
	DictValue* curValue;
	
	array->size++;
	array->values = realloc(array->values, sizeof(DictValue*) * array->size);
	curValue = (DictValue*) malloc(sizeof(StringValue));
	
	curValue->key = (char*) malloc(sizeof("arraykey"));
	strcpy(curValue->key, "arraykey");
	curValue->next = NULL;
	curValue->prev = NULL;
	curValue->type = StringType;
	curValue = (DictValue*) realloc(curValue, sizeof(StringValue));
	((StringValue*)curValue)->value = (char*) malloc(sizeof(char) * (strlen(str) + 1));
	strcpy(((StringValue*)curValue)->value, str);
	
	array->values[array->size - 1] = curValue;
}

void addBoolToDictionary(Dictionary* dict, const char* key, int value) {
	BoolValue* dValue = malloc(sizeof(BoolValue));
	dValue->dValue.type = BoolType;
	dValue->value = value;
	addValueToDictionary(dict, key, (DictValue*) dValue);
}

void addIntegerToDictionary(Dictionary* dict, const char* key, int value) {
	IntegerValue* dValue = malloc(sizeof(IntegerValue));
	dValue->dValue.type = IntegerType;
	dValue->value = value;
	addValueToDictionary(dict, key, (DictValue*) dValue);
}

void addValueToDictionary(Dictionary* dict, const char* key, DictValue* value) {
	value->key = (char*) malloc(sizeof(char) * (strlen(key) + 1));
	strcpy(value->key, key);
	DictValue* curValue = dict->values;
	DictValue* prevValue = NULL;

	while(curValue != NULL) {
		prevValue = curValue;
		curValue = curValue->next;
	}

	value->next = NULL;
	value->prev = prevValue;

	if(prevValue == NULL)
		dict->values = value;
	else
		prevValue->next = value;
}

