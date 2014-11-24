#ifndef _JBOUKER_TABLE_
#define _JBOUKER_TABLE_

typedef struct {
	char ipAdrress[20];
	char nextHopNode[7];
	int if_index;
	int numHops;
	int ts;
} TableEntry;

typedef struct  {
	TableEntry entries[10];
	int size;
} Table;

TableEntry tableEntryMake(char *ipAdrress, char *nextHopNode, int if_index, int numHops, int ts);
void addTableEntry(Table *table, TableEntry entry);
void removeTableEntry(Table *table, char *ipAdrress);
TableEntry * entryForIp(Table *table, char *ipAdrress);
void printTable(Table *table);

#endif
