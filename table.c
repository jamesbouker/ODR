#include "table.h"
#include <string.h>
#include <stdio.h>

int indexForEntry(Table *table, char *ipAdrress) {
	int i;
	int size = table[0].size;

	for(i = 0; i < size; i++) {
		int comp = strcmp(table[0].entries[i].ipAdrress, ipAdrress);
		if(comp == 0)
			return i;
	}

	return -1;
}

void swap(TableEntry * entries, int i, int j) {
	TableEntry temp = entries[i];
	entries[i] = entries[j];
	entries[j] = temp;
}

//tableEntryMake("127.0.0.1", "12345", 1, 10, 762);
TableEntry tableEntryMake(char *ipAdrress, char *nextHopNode, int if_index, int numHops, int ts) {
	TableEntry entry;
	strcpy(entry.ipAdrress, ipAdrress);

	int x;
	for(x = 0; x < 6; x++)
		entry.nextHopNode[x] = nextHopNode[x];

	entry.if_index = if_index;
	entry.numHops = numHops;
	entry.ts = ts;
	return entry;
}

void addTableEntry(Table *table, TableEntry entry) {
	int size = table[0].size;
	table[0].entries[size] = entry;
	table[0].size++;
}

void removeTableEntry(Table *table, char *ipAdrress) {
	int index = indexForEntry(table, ipAdrress);
	int size = table[0].size;

	swap(table[0].entries, index, size-1);
	table[0].size--;
}

TableEntry * entryForIp(Table *table, char *ipAdrress) {
	int index = indexForEntry(table, ipAdrress);
	if(index == -1)
		return NULL;
	else {
		TableEntry *ret = &table[0].entries[index];
		return ret;
	}
}

void printTable(Table *table) {
	int i;
	int size = table[0].size;
	TableEntry *entries = table[0].entries;

	printf("Table Size: %d\n", size);
	for(i = 0; i < size; i++) {
		TableEntry e = entries[i];
		printf("ipAdrress: %s\tnextHopNode: %s\tif_index: %d\tnumHops: %d\n", e.ipAdrress, e.nextHopNode, e.if_index, e.numHops);
	}
}
