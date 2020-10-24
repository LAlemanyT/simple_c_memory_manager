
#include "rc.h"
#include <stdio.h>


static struct strong_ref** rc_graph;
static int length = 0;

struct strong_ref* newRef(size_t n){
	length++;
	rc_graph = (struct strong_ref**)realloc(rc_graph, sizeof(struct strong_ref*) * length);
	rc_graph[length-1] = (struct strong_ref*)malloc(sizeof(struct strong_ref));
	rc_graph[length-1]->ptr = malloc(n);

	rc_graph[length-1]->entry.count = 1;
	rc_graph[length-1]->entry.n_deps = 0;
	rc_graph[length-1]->entry.dep_list = NULL;

	return rc_graph[length-1];
}

struct strong_ref* findRef(void* ptr){
	int i =0;
	while(i<length){
		if(ptr == rc_graph[i]->ptr){
			return rc_graph[i];
		}
		i++;
	}
	return NULL;
}

void addDep(struct strong_ref* ref, struct strong_ref* dep){
	int index =0;
	while(index<length){
		if(dep == rc_graph[index]){
			break;
		}
		index++;
	}

	ref->entry.n_deps++;

	ref->entry.dep_list = realloc(ref->entry.dep_list,sizeof(size_t) * (int)ref->entry.n_deps);
	ref->entry.dep_list[ref->entry.n_deps - 1] = index;
}

/**
 * Returns an allocation of n bytes and creates a reference.
 *
 * If:
 * ptr == NULL && dep == NULL: return new allocation
 *
 * ptr != NULL and an entry exists:
 * Increase the count of the allocation + return a strong reference
 *
 * ptr argument == NULL and deps != NULL:
 * return new allocation with count correlating to passed in dependency
 *   -> once the dependency is deallocated, the count on this object decreases
 *
 * ptr != NULL, dep != NULL, and an entry exists:
 * count of the strong reference increases but is related to the dependency
 * 	 -> once the dependency is deallocated, the count on this object decreases
 */
struct strong_ref* rc_alloc(void* ptr, size_t n, struct strong_ref* dep) {

	if(ptr == NULL && dep == NULL){
		return newRef(n);
	}

	else if(ptr != NULL && dep == NULL){
		struct strong_ref* temp = findRef(ptr);

		if(temp == NULL){
			return NULL;
		}
		temp->entry.count++;

		return temp;
	}

	else if(ptr == NULL && dep != NULL){
		struct strong_ref* temp = newRef(n);
		addDep(temp, dep);
		temp->entry.count = dep->entry.count;
		return temp;

	}

	else if(ptr != NULL && dep != NULL){
		struct strong_ref* temp = findRef(ptr);
		if(ptr==NULL){
			return NULL;
		}
		temp->entry.count+=dep->entry.count;

		addDep(temp, dep);
		return temp;
	}
	return NULL;
}



/**
 * Downgrades strong to weak references,
 * decreases reference count
 *
 * If ref == NULL: returns invalid weak reference
 * If ref is a value that does not exist: returns invalid weak reference
 *
 * invalid weak references are defined as having an entry_id of:
 *   0xFFFFFFFFFFFFFFFF
 *
 * @param strong_ref* ref (reference to allocation)
 * @return weak_ref (reference with an entry id)
 */
struct weak_ref rc_downgrade(struct strong_ref* ref) {

	if(ref == NULL){
		struct weak_ref r = { RC_INVALID_REF };
		return r;
	}

	if(ref->entry.count == 0){
		struct weak_ref r = { RC_INVALID_REF };
		return r;
	}

	int found = 0;
	int index = 0;
	while(index<length){
		if(ref->ptr == rc_graph[index]->ptr){
			found=1;
			break;
		}
		index++;
	}

	if(!found){
		struct weak_ref r = { RC_INVALID_REF };
		return r;
	}


	ref->entry.count--;
	if(ref->entry.count == 0){
		struct weak_ref r = { RC_INVALID_REF };
		return r;
	}

	struct weak_ref r = { index };
	return r;
}


/**
 * Upgrdes a weak to strong references
 * Ensures weak reference is valid
 * If the matching strong reference no longer exists or has been deallocated:
 * returns NULL
 */
struct strong_ref* rc_upgrade(struct weak_ref ref) {

	if(ref.entry_id == RC_INVALID_REF || ref.entry_id>=length){
		return NULL;
	}
	struct strong_ref* temp= rc_graph[ref.entry_id];
	if(temp->entry.count == 0){
		return NULL;
	}

	else{
		int i =0;
		while(i<(int)temp->entry.n_deps){
			if(rc_graph[temp->entry.dep_list[i]] == 0){
				return NULL;
			}
			i++;
		}

		temp->entry.count++;
		return temp;
	}
}


/**
 * Frees all allocated memory
 */
void rc_cleanup() {
	int i = 0;
	while(i<length){
		if(rc_graph[i]->entry.dep_list>0){
			free(rc_graph[i]->entry.dep_list);
		}

		free(rc_graph[i]->ptr);
		free(rc_graph[i]);
		i++;
	}
	length = 0;
	free(rc_graph);
}
