/*
 *Author: tdnguyen
 Simple map table implementation
 Each element in the map table is a pair of (filename, offset)
 * */
#ifndef __MSSD_H__
#define __MSSD_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h> //for struct timeval, gettimeofday()
#include <string.h>
#include <stdint.h> //for uint64_t
#include <math.h> //for log()
#include <assert.h>
#define MSSD_MAX_FILE 20
#define MSSD_MAX_FILE_NAME_LENGTH 256
//#define MSSD_THREAD_TRIGGER_FRACTION 0.1
#define MSSD_THREAD_TRIGGER_FRACTION 0.5
#define MSSD_THREAD_TRIGGER_MIN 10 


/*BECAREFUL to decide this value, this effect to number of opened streamds (+/- 1) MSSD_COLL_INIT_SID, MSSD_IDX_INIT_SID
 *Number of stream need to open = MSSD_OPLOG_SID + 2k
 * */


//streamd id const
#define MSSD_UNDEFINED_SID -1

#if defined (S840_PRO)
#define MSSD_OTHER_SID 0 
#else //Samsung PM953
#define MSSD_OTHER_SID 1 
#endif

#define MSSD_JOURNAL_SID (MSSD_OTHER_SID + 1) //journal files need to be in seperated stream

#if defined(USE_OPLOG)
#define MSSD_OPLOG_SID (MSSD_JOURNAL_SID + 1) //oplog collection  need to be in seperated stream
#else
#define MSSD_OPLOG_SID  (MSSD_JOURNAL_SID)
#endif //defined(USE_OPLOG)

//#define MSSD_OPLOG_SID (MSSD_PRIMARY_IDX_SID + 1) //oplog collection  need to be in seperated stream

/*
 *Note about choosing ALPHA values
 ALPHA use to seperate very hot | very cold and the remains are warm data
 Each hot and cold occupy 1/ALPHA ratio in the total range. The remain warm groups occuy (ALPHA - 2) / K parts equally
 * */
#if defined (SSDM_OP11)
	//for general k groups DSM
	/*MSSD_NUM_GROUP should >= 3
	 * 2 + 2 * MSSD_NUM_GROUP <= total stream supported by SSD.
	 * e.g. Samsung PM953 support 16 streams => MSSD_NUM_GROUP <= 7
	 *When MSSD_NUM_GROUP == 3, SSDM_OP11 becomes SSDM_OP8
	 * */
//	#define MSSD_NUM_GROUP 4 
	#define MSSD_NUM_GROUP 5 
	#define MSSD_NUM_P (MSSD_NUM_GROUP - 1)
	//collection sids from 3 ~ 3 + MSSD_NUM_GROUP
#if defined (S840_PRO)
	#define MSSD_PRIMARY_IDX_SID (MSSD_OPLOG_SID + 0) //primary index files have seq write IO, thus should be in seperated stream
	#define MSSD_COLL_INIT_SID (MSSD_OPLOG_SID + 1)  
#else //Samsung PM953
	#define MSSD_PRIMARY_IDX_SID (MSSD_OPLOG_SID + 1) //primary index files have seq write IO, thus should be in seperated stream
	#define MSSD_COLL_INIT_SID (MSSD_OPLOG_SID + 2)  
#endif //defined (S840_PRO)
	//index sids from (3 + MSSD_NUM_GROUP + 1) ~ (3 + MSSD_NUM_GROUP + 1 + MSSD_NUM_GROUP)
	#define MSSD_IDX_INIT_SID (MSSD_COLL_INIT_SID + MSSD_NUM_GROUP)   
	//For hotness compute
	// MSSD_NUM_GROUP : ALPHA recommended: 3:6, 4:8
	#define ALPHA 8 
	#define THRESHOLD1 0.05
	//#define THRESHOLD2 1 //skip node primary index
	#define THRESHOLD2 10 //not skip any primary index
	#define MSSD_RECOVER_TIME 100
#elif defined (SSDM_OP6)
	/*
	 * Very simple boundary-based approach
	 * skip streamid for primary index
	 * 1: other, 2: journal, (3: oplog)
	 * 4: collection-left
	 * 5: collection-right
	 * 6: index-left
	 * 7: index-right 
	 * */
	#define MSSD_COLL_INIT_SID (MSSD_OPLOG_SID + 1)  
	#define MSSD_IDX_INIT_SID (MSSD_COLL_INIT_SID + 2)  
#elif defined (SSDM_OP10)
	#if defined(S840_PRO)
		//others: 0 , journal: 1, (oplog (2)), all colls: 2 (3), all indexes: 3 (4)
		#define MSSD_COLL_INIT_SID (MSSD_OPLOG_SID + 1)  
		#define MSSD_IDX_INIT_SID (MSSD_COLL_INIT_SID + 1)  
	#else //Samsung PM953
		//do nothing
	#endif

#else //SSDM_OP8, special case of k groups with k = 3

#if defined(S840_PRO)
	//other: 0, journal, OPLOG, primary: 1, coll: 2~4, idx:5~7
	#define MSSD_PRIMARY_IDX_SID (MSSD_OPLOG_SID + 0) //primary index files have seq write IO, thus should be in seperated stream
	#define MSSD_COLL_INIT_SID (MSSD_OPLOG_SID + 2)  
#else //Samsung PM953
	//other: 1, journal: 2, OPLOG:3, primary:4, coll:5~7, idx: 8~10
	#define MSSD_PRIMARY_IDX_SID (MSSD_OPLOG_SID + 1) //primary index files have seq write IO, thus should be in seperated stream
	#define MSSD_COLL_INIT_SID (MSSD_OPLOG_SID + 3)  
#endif
	#define MSSD_IDX_INIT_SID (MSSD_COLL_INIT_SID + 3)  
	//For hotness compute
	//#define ALPHA 3 
	//#define ALPHA 3 
	#define ALPHA 6
	#define THRESHOLD1 0.05
	//#define THRESHOLD1 0 //skip any index
	//
	//#define THRESHOLD2 1 //skip node primary index
	#define THRESHOLD2 10 //not skip any primary index
	//#define THRESHOLD2 0 //skip any index

	#define MSSD_RECOVER_TIME 100
#endif


#define MSSD_CKPT_MODE 1
#define MSSD_CHECK_MODE 2



/*(file_name, offset) pair used for multi-streamed ssd
 *For multiple collection files and index files, fixed-single boundary is not possible 
 Each collection file or index file need a boundary. 
 Boundaries should get by code (not manually)
 * */
#if defined(SSDM_OP8) || defined (SSDM_OP8_2) || defined (SSDM_OP10) || defined (SSDM_OP11)
typedef struct __mssd_pair {
	char* fn; //file name
	off_t offset; //last physical offset of a file 
	size_t num_w1; //number of write on area 1
	size_t num_w2; //number of write on area 2 
	//ranges of writes within the interval time (e.g. checkpoint interval)
	off_t off_min1;
	off_t off_max1;
	off_t off_min2;
	off_t off_max2;
	//support variable for compute hotness
	double gpct1;// global percentage 1
	double gpct2;// global percentage 2
	double ws1; //write speed 1
	double ws2; //write speed 2
	//stream ids 
	int cur_sid; //current sid
	int sid1;
	int sid2;
	int prev_tem_sid1;
	int prev_tem_sid2;
#if defined (SSDM_OP8_2)
	int prev_sid1;
	int prev_sid2;
	int prev_prev_sid1;
	int prev_prev_sid2;
#endif 
	//precision stat
	int err_count;
	
} __mssd_pair;
typedef struct __mssd_pair MSSD_PAIR;

typedef struct __mssd_map {
	__mssd_pair** data;	
	int size; //current number of pairs
	struct timeval tv; //time when the check is called
	bool is_recovery;
} __mssd_map;
typedef __mssd_map MSSD_MAP;
#elif defined(SSDM_OP9)
typedef struct __mssd_pair {
	char* fn; //file name
	off_t offset; //last physical offset of a file 
	size_t num_w1; //number of write on area 1
	size_t num_w2; //number of write on area 2 
	//ranges of writes within the interval time (e.g. checkpoint interval)
	off_t off_min1;
	off_t off_max1;
	off_t off_min2;
	off_t off_max2;
	//support variable for compute hotness
	double gpct1;// global percentage 1
	double gpct2;// global percentage 2
	double ws1; //write speed 1
	double ws2; //write speed 2
	//stream ids 
	int cur_sid; //current sid
	int sid1;
	int sid2;
	int prev_tem_sid1;
	int prev_tem_sid2;
	int ckpt_sid1;
	int ckpt_sid2;
	int mid_sid1;
	int mid_sid2;
	//precision stat
	int err_count;
} __mssd_pair;
typedef struct __mssd_pair MSSD_PAIR;

typedef struct __mssd_map {
	__mssd_pair** data;	
	int size; //current number of pairs
	struct timeval ckpt_tv; //time when the checkpoint is called
	struct timeval tv; //time when the checkpoint is called
	double duration; //checkpoint interval
	bool is_recovery;
} __mssd_map;
typedef __mssd_map MSSD_MAP;
#else //MSSD_OP6
typedef struct __mssd_pair {
	char* fn; //file name
	off_t offset; //last physical offset of a file 
} __mssd_pair;
typedef struct __mssd_pair MSSD_PAIR;

typedef struct __mssd_map {
	__mssd_pair** data;	
	int size; //current number of pairs
	bool is_recovery;
} __mssd_map;
typedef __mssd_map MSSD_MAP;
#endif


/*
struct __mssd_pair {
	char* fn; //file name
	off_t offset; //last physical offset of a file 
};

struct __mssd_map {
	__mssd_pair** data;	
	int size; //current number of pairs
};
*/

/*
MSSD_MAP* mssdmap_new();
void mssdmap_free(MSSD_MAP* m);
int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, off_t* retval);
int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val);
int mssdmap_find(MSSD_MAP* m, const char* key);
off_t mssdmap_get_offset_by_id(MSSD_MAP* m, int id);
char* mssdmap_get_filename_by_id(MSSD_MAP* m, int id);
int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val);
*/

static inline MSSD_MAP* mssdmap_new();
static inline void mssdmap_free(MSSD_MAP* m);
static inline int mssdmap_find(MSSD_MAP* m, const char* key);
static inline off_t mssdmap_get_offset_by_id(MSSD_MAP* m, int id);
static inline char* mssdmap_get_filename_by_id(MSSD_MAP* m, int id);
#if defined (SSDM_OP8) || defined (SSDM_OP8_2) || defined (SSDM_OP10) || defined (SSDM_OP11)
static inline int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, const int sid, off_t* retval);
static inline int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val,const int sid);
static inline int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val, const int sid);
static inline void mssdmap_flexmap(MSSD_MAP *m, FILE* fp);
static inline void mssdmap_stat_report(MSSD_MAP* m, FILE* fp);
#elif defined (SSDM_OP9)
static inline int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, const int sid, off_t* retval);
static inline int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val,const int sid);
static inline int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val, const int sid);
static inline void mssdmap_flexmap(MSSD_MAP *m, FILE* fp, int mode);
static inline void mssdmap_ckpt_check(MSSD_MAP* m, FILE* fp);
static inline void mssdmap_stat_report(MSSD_MAP* m, FILE* fp);
#else
static inline int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, off_t* retval);
static inline int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val);
static inline int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val);
#endif //SSDM_OP8

#if defined(SSDM_OP8) || defined(SSDM_OP8_2) ||defined(SSDM_OP9) || defined (SSDM_OP10) || defined (SSDM_OP11)
//MSSD_MAP* mssdmap_new() {
static inline MSSD_MAP* mssdmap_new() {
	MSSD_MAP* m = (MSSD_MAP*) malloc(sizeof(MSSD_MAP));
	if(!m) goto err;

	m->data = (MSSD_PAIR**) calloc(MSSD_MAX_FILE, sizeof(MSSD_PAIR*));
	if(!m->data) goto err;
    m->is_recovery=true;	
	m->size = 0;
	gettimeofday(&m->tv, NULL);
#if defined(SSDM_OP9)
	gettimeofday(&m->ckpt_tv, NULL);
#endif 
	return m;

err:
	if (m)
		mssdmap_free(m);
	return NULL;
}
#else
//MSSD_MAP* mssdmap_new() {
static inline MSSD_MAP* mssdmap_new() {
	MSSD_MAP* m = (MSSD_MAP*) malloc(sizeof(MSSD_MAP));
	if(!m) goto err;

	m->data = (MSSD_PAIR**) calloc(MSSD_MAX_FILE, sizeof(MSSD_PAIR*));
	if(!m->data) goto err;
	
	m->size = 0;
	return m;

err:
	if (m)
		mssdmap_free(m);
	return NULL;
}
#endif //SSDM_OP8
// void mssdmap_free(MSSD_MAP* m) {
static inline void mssdmap_free(MSSD_MAP* m) {
	int i;
	if (m->size > 0) {
		for (i = 0; i < m->size; i++) {
			free(m->data[i]->fn);
			free(m->data[i]);
		}
	}
	free(m->data);
	free(m);
}
/* Main function use for multi-streamed SSD 
 * input: key, value
 * If key is exist in map table => get the offset, retval will be set to 0; return the id of exist key
 * Else, append (key, value); return the last id 
 * * */
#if defined (SSDM_OP8) || defined(SSDM_OP8_2) || defined (SSDM_OP9) || defined(SSDM_OP10) || defined(SSDM_OP11)
static inline int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, const int sid, off_t* retval) {
	int id;
	id = mssdmap_find(m, key);
	if (id >= 0){
		*retval = m->data[id]->offset;
		return id;
	}	
	mssdmap_append(m, key, val, sid);
	*retval = 0;
	return (m->size - 1);
}
/* * input: key, value
 * If key is exist in map table => update its value
 * Else, append (key, value) 
 * * */
//int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val) {
static inline int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val, const int sid) {
	int id;
	id = mssdmap_find(m, key);
	if (id >= 0){
		m->data[id]->offset = val;	
		return id;
	}	
	mssdmap_append(m, key, val, sid);
	return (m->size - 1);
}
#else //other medthods MSSD_OP6, MSSD_OP7
//int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, off_t* retval) {
static inline int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, off_t* retval) {
	int id;
	id = mssdmap_find(m, key);
	if (id >= 0){
		*retval = m->data[id]->offset;
		return id;
	}	
	mssdmap_append(m, key, val);
	*retval = 0;
	return (m->size - 1);
}
/* * input: key, value
 * If key is exist in map table => update its value
 * Else, append (key, value) 
 * * */
//int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val) {
static inline int mssdmap_set_or_append(MSSD_MAP* m, const char* key, const off_t val) {
	int id;
	id = mssdmap_find(m, key);
	if (id >= 0){
		m->data[id]->offset = val;	
		return id;
	}	
	mssdmap_append(m, key, val);
	return (m->size - 1);
}
#endif //SSDM_OP8


/* find key and return index in the array
 * Just simple scan whole items.
 * The number of files are expected small
 *return -1 if key is not exist
 * */
//int mssdmap_find(MSSD_MAP* m, const char* key){
static inline int mssdmap_find(MSSD_MAP* m, const char* key){
	int i;
	for (i = 0; i < m->size; i++){
		if (strcmp(m->data[i]->fn, key) == 0)
			return i;
	}
	return -1;
}
//off_t mssdmap_get_offset_by_id(MSSD_MAP* m, int id) {
static inline off_t mssdmap_get_offset_by_id(MSSD_MAP* m, int id) {
	return (m->data[id]->offset);
}
//char* mssdmap_get_filename_by_id(MSSD_MAP* m, int id){
static inline char* mssdmap_get_filename_by_id(MSSD_MAP* m, int id){
	return (m->data[id]->fn);
}
/*
 *Create new MSSD_PAIR based on input key, val and append on the list
 * */
#if defined(SSDM_OP8) || defined(SSDM_OP8_2) || defined(SSDM_OP9) || defined (SSDM_OP10) || defined (SSDM_OP11)
//int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val) {
static inline int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val, const int sid) {
	if (m->size >= MSSD_MAX_FILE) {
		printf("mssdmap is full!\n");
		return -1;
	}
	MSSD_PAIR* pair = (MSSD_PAIR*) malloc(sizeof(MSSD_PAIR));
	pair->fn = (char*) malloc(MSSD_MAX_FILE_NAME_LENGTH);
	strcpy(pair->fn, key);
	pair->offset = val;
	pair->num_w1 = pair->num_w2 = 0;
	pair->off_min1 = pair->off_min2 = 100 * val;
	pair->off_max1 = pair->off_max2 = 0;
	pair->sid1 = pair->cur_sid = sid;
	pair->sid2 = sid + 1;
	pair->prev_tem_sid1 = MSSD_UNDEFINED_SID;
	pair->prev_tem_sid2 = MSSD_UNDEFINED_SID;

	pair->err_count = 0;

#if defined(SSDM_OP8_2)
	pair->prev_sid1 = pair->prev_sid2 = pair->prev_prev_sid1 = pair->prev_prev_sid2 = MSSD_UNDEFINED_SID;
#endif 
#if defined(SSDM_OP9)
	pair->ckpt_sid1 = pair->sid1;
	pair->ckpt_sid2 = pair->sid2;
#endif
	
	m->data[m->size] = pair;
	m->size++;
	return 0;
}
#else //normal (SSDM_OP6, SSDM_OP7)
//int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val) {
static inline int mssdmap_append(MSSD_MAP* m, const char* key, const off_t val) {
	if (m->size >= MSSD_MAX_FILE) {
		printf("mssdmap is full!\n");
		return -1;
	}
	MSSD_PAIR* pair = (MSSD_PAIR*) malloc(sizeof(MSSD_PAIR));
	pair->fn = (char*) malloc(MSSD_MAX_FILE_NAME_LENGTH);
	strcpy(pair->fn, key);
	pair->offset = val;
	
	m->data[m->size] = pair;
	m->size++;
	return 0;
}
#endif //SSDM_OP8

#if defined(SSDM_OP8) || defined(SSDM_OP11)
/*
 * Predict next streamid will be used for each part of an obj, based on current value in this checkpoint
 * An object here is a collection file or index file
 * tem_sid1, tem_sid2: the streamd id computed based on statistic information in this checkpoint (trusted value)
 * */
static inline void mssdmap_predict_stream(MSSD_PAIR* obj, const int tem_sid1, const int tem_sid2) {
	if(obj->prev_tem_sid1 == tem_sid1){
		//if the current hot-cold trend is same, do not swap
		obj->sid1 = tem_sid1;
	}
	else {
		//now assign new stream, just simple swap
		obj->sid1 = tem_sid2;
	}

	if(obj->prev_tem_sid2 == tem_sid2){
		//if the current hot-cold trend is same, do not swap
		obj->sid2 = tem_sid2;
	}
	else {
		//now assign new stream, just simple swap
		obj->sid2 = tem_sid1;
	}
}
#endif 

#if defined(SSDM_OP8) || defined(SSDM_OP8_2)
/*
 *In SSDM_OP8, this function is only called when checkpoint 
 All computations are based on the interval checkpoint time 
 * */
static inline void mssdmap_flexmap(MSSD_MAP *m, FILE* fp){

	int i;
	int tem_sid1, tem_sid2;
	MSSD_PAIR* obj;
	bool is_pidx;
	double den1, den2, local_pct1, local_pct2, global_pct1, global_pct2, wpps1, wpps2;
	struct timeval tv_tem;
	double time_s;
	double coll_min, coll_max, idx_min, idx_max;
	double coll_p1, coll_p2, idx_p1, idx_p2;
	size_t coll_count1, coll_count2, idx_count1, idx_count2; //counts for global cpt computation
	coll_count1 = coll_count2 = idx_count1 = idx_count2 = 0;
	coll_min = idx_min = 100000;
	coll_max = idx_max = -100000;
	coll_max = -100000;
	tem_sid1 = tem_sid2 = MSSD_UNDEFINED_SID;

//compute the ckpt interval time in seconds and update
	gettimeofday(&tv_tem, NULL);
	time_s = (tv_tem.tv_sec - m->tv.tv_sec);
	if(time_s <= 0){
				printf("error! ckpt interval <= 0");	
				return;
	}
	m->tv = tv_tem;

//phase 1: compute the total write in a stream id
		for( i = 0; i < m->size; i++){
			obj = m->data[i];
			if(strstr(obj->fn, "collection") != 0){
				coll_count1 += obj->num_w1;
				coll_count2 += obj->num_w2;

			}
			else if(strstr(obj->fn, "index") != 0) {
				idx_count1 += obj->num_w1;	
				idx_count2 += obj->num_w2;	
			}
		}
//phase 2: compute densities, write per page per second,  and written percentages
		for( i = 0; i < m->size; i++){
			obj = m->data[i];

			den1 = (obj->off_max1 > obj->off_min1) ? ((obj->num_w1 * 4096.0) / (obj->off_max1 - obj->off_min1)) : (-1) ;
			den2 = (obj->off_max2 > obj->off_min2) ? ((obj->num_w2 * 4096.0) / (obj->off_max2 - obj->off_min2)) : (-1) ;
			if(time_s > 0){
				wpps1 = den1 / time_s;
				wpps2 = den2 / time_s;
				//Compute logarit
				obj->ws1 = log(wpps1) / log(10);
				obj->ws2 = log(wpps2) / log(10);
			}


			if(strstr(obj->fn, "collection") != 0){
				obj->gpct1 = global_pct1 = (obj->num_w1 * 1.0) / coll_count1 * 100;
				obj->gpct2 = global_pct2 = (obj->num_w2 * 1.0) / coll_count2 * 100;
				//compute min, max
				if(obj->ws1 < coll_min)
					coll_min = obj->ws1;
				if(obj->ws2 < coll_min)
					coll_min = obj->ws2;
				if(obj->ws1 > coll_max)
					coll_max = obj->ws1;
				if(obj->ws2 > coll_max)
					coll_max = obj->ws2;
			}
			else if(strstr(obj->fn, "index") != 0) {
				obj->gpct1 = global_pct1 = (obj->num_w1 * 1.0) / idx_count1 * 100;
				obj->gpct2 = global_pct2 = (obj->num_w2 * 1.0) / idx_count2 * 100;
				//compute min, max, except index files that has global percentage too small 
				if(global_pct1 + global_pct2 > THRESHOLD2) {
					if(global_pct1 > THRESHOLD1){
						if(obj->ws1 < idx_min)
							idx_min = obj->ws1;
						if(obj->ws1 > idx_max)
							idx_max = obj->ws1;
					}
					if(global_pct2 > THRESHOLD1){
						if(obj->ws2 < idx_min)
							idx_min = obj->ws2;
						if(obj->ws2 > idx_max)
							idx_max = obj->ws2;
					}
				}
			}

		}//end for
//phase 3: stream mapping based on hotness
		//compute pivot points
		coll_p1	= coll_min + ((coll_max - coll_min) * (1.0 / ALPHA) );
		coll_p2 = coll_min + ( (coll_max - coll_min) * ((ALPHA-1)*1.0/ALPHA) );
		idx_p1	= idx_min + ((idx_max - idx_min) * (1.0 / ALPHA) );
		idx_p2 = idx_min + ( (idx_max - idx_min) * ((ALPHA-1)*1.0/ALPHA) );
		//mapping
		//(1): check range: we will check the range that the wsi belong to and assign the stream-id depend which range it belongs to 
		//(2): swap sid of left and right part
		for( i = 0; i < m->size; i++){
			obj = m->data[i];
			local_pct1 = (obj->num_w1 * 1.0) / (obj->num_w1 + obj->num_w2) * 100;
			local_pct2 = (obj->num_w2 * 1.0) / (obj->num_w1 + obj->num_w2) * 100;

			if(strstr(obj->fn, "collection") != 0){
				//(1) compute suggested stream id
				if(obj->ws1 <= coll_p1){
					tem_sid1 = MSSD_COLL_INIT_SID - 1;
				}
				else if(coll_p1 < obj->ws1 && obj->ws1 <= coll_p2){
					tem_sid1 = MSSD_COLL_INIT_SID;
				}
				else{
					tem_sid1 = MSSD_COLL_INIT_SID + 1;
				}

				if(obj->ws2 <= coll_p1){
					tem_sid2 = MSSD_COLL_INIT_SID - 1;
				}
				else if(coll_p1 < obj->ws2 && obj->ws2 <= coll_p2){
					tem_sid2 = MSSD_COLL_INIT_SID;
				}
				else{
					tem_sid2 = MSSD_COLL_INIT_SID + 1;
				}

#if defined(SSDM_OP8_DEBUG)
				printf("__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, coll_min, coll_p1, coll_p2, coll_max);
				fprintf(fp, "__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, coll_min, coll_p1, coll_p2, coll_max);
#endif
				//(2) stream mapping
				//if (time_s > MSSD_RECOVER_TIME){	
				if (!m->is_recovery){	
					//error collection, when the prediction is wrong, increase the error count
					if (obj->sid1 != tem_sid1)
						obj->err_count++;
					if (obj->sid2 != tem_sid2)
						obj->err_count++;
/*
 *Prediction rule:
 (1) If the current tem_sid == prev_tem_sid => the streamid will repeat in next ckpt
 (2) Else. Choose from two predictors
    (2.1): Use swap stream id from this check point
	(2.2): Use streamid from previous ckpt
 * */
#if defined(SSDM_OP8)

					//predict the streams will be used in next checkpoint
					mssdmap_predict_stream(obj, tem_sid1, tem_sid2);

#elif defined(SSDM_OP8_2)
			        if ((obj->prev_prev_sid1 != MSSD_UNDEFINED_SID)  && 	
							(obj->prev_prev_sid2 != MSSD_UNDEFINED_SID)) {
						//learn from the previous of previous checkpoint 
						obj->sid1 = obj->prev_prev_sid1;
						obj->sid2 = obj->prev_prev_sid2;

					}
					else {
						//It is the first checkpoint, use the same way with swap method
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
					//update 
					obj->prev_prev_sid1 = obj->prev_sid1;
					obj->prev_prev_sid2 = obj->prev_sid2;
					obj->prev_sid1 = tem_sid1;
					obj->prev_sid2 = tem_sid2;
				
#endif
				} //end time_s > MSSD_RECOVER_TIME
			}
			else if(strstr(obj->fn, "index") != 0) {
				//(1) compute suggested stream id
				//detect primary index, primary index should be seperated 
				is_pidx = (obj->gpct1 < THRESHOLD1 || obj->gpct2 < THRESHOLD1 ||
							(obj->gpct1 + obj->gpct2 < THRESHOLD2));

				if(is_pidx){
					tem_sid1 = tem_sid2 = MSSD_PRIMARY_IDX_SID;
					//tem_sid1 = tem_sid2 = MSSD_OTHER_SID;
				}
				else {
					if(obj->ws1 <= idx_p1){
						tem_sid1 = MSSD_IDX_INIT_SID - 1;
					}
					else if(idx_p1 < obj->ws1 && obj->ws1 <= idx_p2){
						tem_sid1 = MSSD_IDX_INIT_SID;
					}
					else{
						tem_sid1 = MSSD_IDX_INIT_SID + 1;
					}

					if(obj->ws2 <= idx_p1){
						tem_sid2 = MSSD_IDX_INIT_SID - 1;
					}
					else if(idx_p1 < obj->ws2 && obj->ws2 <= idx_p2){
						tem_sid2 = MSSD_IDX_INIT_SID;
					}
					else{
						tem_sid2 = MSSD_IDX_INIT_SID + 1;
					}
				}

#if defined(SSDM_OP8_DEBUG)
				printf("__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, idx_min, idx_p1, idx_p2, idx_max);
				fprintf(fp, "__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, idx_min, idx_p1, idx_p2, idx_max);
#endif
				//(2) stream mapping
				//error collection, when the prediction is wrong, increase the error count
				if (obj->sid1 != tem_sid1)
					obj->err_count++;
				if (obj->sid2 != tem_sid2)
					obj->err_count++;

				//if (time_s < MSSD_RECOVER_TIME) {
				if (m->is_recovery) {
					//This is special case, for handling early detect primary index  
					if(is_pidx) {
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
				}
				else {
#if defined(SSDM_OP8)
					//predict the streams will be used in next checkpoint
				    mssdmap_predict_stream (obj, tem_sid1, tem_sid2);

					/*
					if ( (obj->prev_tem_sid1 == tem_sid1) && (obj->prev_tem_sid2 == tem_sid2) ){
						obj->sid1 = tem_sid1;
						obj->sid2 = tem_sid2;
					}
					else {
						//simple swap 
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
					*/
#elif defined(SSDM_OP8_2)
			        if ((obj->prev_prev_sid1 != MSSD_UNDEFINED_SID)  && 	
							(obj->prev_prev_sid2 != MSSD_UNDEFINED_SID)) {
						//learn from the previous of previous checkpoint 
						obj->sid1 = obj->prev_prev_sid1;
						obj->sid2 = obj->prev_prev_sid2;

					}
					else {
						//It is the first checkpoint, use the same way with swap method
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
					//update 
					obj->prev_prev_sid1 = obj->prev_sid1;
					obj->prev_prev_sid2 = obj->prev_sid2;
					obj->prev_sid1 = tem_sid1;
					obj->prev_sid2 = tem_sid2;
#endif
				}
			} //end else index

			//update current trends
			obj->prev_tem_sid1 = tem_sid1;
			obj->prev_tem_sid2 = tem_sid2;
			//reset this obj's metadata for next ckpt
			obj->num_w1 = obj->num_w2 = 0;
			obj->off_min1 = obj->off_min2 = 100 * obj->offset;
			obj->off_max1 = obj->off_max2 = 0;
		}//end for
	
	if (m->is_recovery)
		m->is_recovery = false;
}
#endif

#if defined(SSDM_OP9)
//simply just update the duration of checkpoint 
static inline void mssdmap_ckpt_check(MSSD_MAP* m, FILE* fp){
	struct timeval tv_tem;
	double time_s;
	gettimeofday(&tv_tem, NULL);
	time_s = (tv_tem.tv_sec - m->tv.tv_sec);
	//call mssdmap_flexmap() ???
	
	m->duration = time_s;
	m->tv = tv_tem;
}
//flexible map streamd ids based on obj's hotness
static inline void mssdmap_flexmap(MSSD_MAP *m, FILE* fp, int mode){
	int i;
	int tem_sid1, tem_sid2;
	MSSD_PAIR* obj;
	bool is_pidx;
	double den1, den2, local_pct1, local_pct2, global_pct1, global_pct2, wpps1, wpps2;
	struct timeval tv_tem;
	double time_s;
	double coll_min, coll_max, idx_min, idx_max;
	double coll_p1, coll_p2, idx_p1, idx_p2;
	size_t coll_count1, coll_count2, idx_count1, idx_count2; //counts for global cpt computation
	coll_count1 = coll_count2 = idx_count1 = idx_count2 = 0;
	coll_min = idx_min = 100000;
	coll_max = idx_max = -100000;
	coll_max = -100000;

	//compute the interval time in seconds
	//case 1: if this function is called at checkpoint time, time_s is checkpoint interval
	//case 2: if this function is called at online-check, time_s is the normal interval
	gettimeofday(&tv_tem, NULL);
	if(mode == MSSD_CKPT_MODE) {
		m->duration = (tv_tem.tv_sec - m->ckpt_tv.tv_sec);
		m->ckpt_tv = tv_tem;
	}
	time_s = (tv_tem.tv_sec - m->tv.tv_sec);
	m->tv = tv_tem;


	if(time_s <= 0){
		printf("error! ckpt interval <= 0");	
		return;
	}
	//update timeval for both checkpoint and online-check
	//m->tv = tv_tem;
	//phase 1: compute the total write in a stream id
	for( i = 0; i < m->size; i++){
		obj = m->data[i];
		if(strstr(obj->fn, "collection") != 0){
			coll_count1 += obj->num_w1;
			coll_count2 += obj->num_w2;

		}
		else if(strstr(obj->fn, "index") != 0) {
			idx_count1 += obj->num_w1;	
			idx_count2 += obj->num_w2;	
		}
	}
	//phase 2: compute densities, write per page per second,  and written percentages
	for( i = 0; i < m->size; i++){
		obj = m->data[i];

		den1 = (obj->off_max1 > obj->off_min1) ? ((obj->num_w1 * 4096.0) / (obj->off_max1 - obj->off_min1)) : (-1) ;
		den2 = (obj->off_max2 > obj->off_min2) ? ((obj->num_w2 * 4096.0) / (obj->off_max2 - obj->off_min2)) : (-1) ;
		if(time_s > 0){
			wpps1 = den1 / time_s;
			wpps2 = den2 / time_s;
			//Compute logarit
			obj->ws1 = log(wpps1) / log(10);
			obj->ws2 = log(wpps2) / log(10);
		}


		if(strstr(obj->fn, "collection") != 0){
			obj->gpct1 = global_pct1 = (obj->num_w1 * 1.0) / coll_count1 * 100;
			obj->gpct2 = global_pct2 = (obj->num_w2 * 1.0) / coll_count2 * 100;
			//compute min, max
			if(obj->ws1 < coll_min)
				coll_min = obj->ws1;
			if(obj->ws2 < coll_min)
				coll_min = obj->ws2;
			if(obj->ws1 > coll_max)
				coll_max = obj->ws1;
			if(obj->ws2 > coll_max)
				coll_max = obj->ws2;
		}
		else if(strstr(obj->fn, "index") != 0) {
			obj->gpct1 = global_pct1 = (obj->num_w1 * 1.0) / idx_count1 * 100;
			obj->gpct2 = global_pct2 = (obj->num_w2 * 1.0) / idx_count2 * 100;
			//compute min, max, except index files that has global percentage too small 
			if(global_pct1 + global_pct2 > THRESHOLD2) {
				if(global_pct1 > THRESHOLD1){
					if(obj->ws1 < idx_min)
						idx_min = obj->ws1;
					if(obj->ws1 > idx_max)
						idx_max = obj->ws1;
				}
				if(global_pct2 > THRESHOLD1){
					if(obj->ws2 < idx_min)
						idx_min = obj->ws2;
					if(obj->ws2 > idx_max)
						idx_max = obj->ws2;
				}
			}
		}

	}//end for
	//phase 3: stream mapping based on hotness
	//compute pivot points
	coll_p1	= coll_min + ((coll_max - coll_min) * (1.0 / ALPHA) );
	coll_p2 = coll_min + ( (coll_max - coll_min) * ((ALPHA-1)*1.0/ALPHA) );
	idx_p1	= idx_min + ((idx_max - idx_min) * (1.0 / ALPHA) );
	idx_p2 = idx_min + ( (idx_max - idx_min) * ((ALPHA-1)*1.0/ALPHA) );
	//mapping
	//(1): check range: we will check the range that the wsi belong to and assign the stream-id depend which range it belongs to 
	//(2): swap sid of left and right part
	for( i = 0; i < m->size; i++){
		obj = m->data[i];
		local_pct1 = (obj->num_w1 * 1.0) / (obj->num_w1 + obj->num_w2) * 100;
		local_pct2 = (obj->num_w2 * 1.0) / (obj->num_w1 + obj->num_w2) * 100;
		
		if(strstr(obj->fn, "collection") != 0){
			//(1) compute current stream suggestion based on info within this checkpoint
			if(obj->ws1 <= coll_p1){
				tem_sid1 = MSSD_COLL_INIT_SID - 1;
			}
			else if(coll_p1 < obj->ws1 && obj->ws1 <= coll_p2){
				tem_sid1 = MSSD_COLL_INIT_SID;
			}
			else{
				tem_sid1 = MSSD_COLL_INIT_SID + 1;
			}

			if(obj->ws2 <= coll_p1){
				tem_sid2 = MSSD_COLL_INIT_SID - 1;
			}
			else if(coll_p1 < obj->ws2 && obj->ws2 <= coll_p2){
				tem_sid2 = MSSD_COLL_INIT_SID;
			}
			else{
				tem_sid2 = MSSD_COLL_INIT_SID + 1;
			}
#if defined(SSDM_OP9_DEBUG)
			printf("__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, coll_min, coll_p1, coll_p2, coll_max);
			fprintf(fp, "__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, coll_min, coll_p1, coll_p2, coll_max);
#endif
			//(2) map stream
			//if(time_s > MSSD_RECOVER_TIME){
			if(!m->is_recovery){
				//error collection, when the prediction is wrong, increase the error count
				if (obj->sid1 != tem_sid1)
					obj->err_count++;
				if (obj->sid2 != tem_sid2)
					obj->err_count++;

				if (mode == MSSD_CKPT_MODE) {
					//simple swap from last previous checkpoint
					obj->sid1 = obj->mid_sid2;
					obj->sid2 = obj->mid_sid1;
					//save the suggested streams 
					obj->ckpt_sid1 = tem_sid1;
					obj->ckpt_sid2 = tem_sid2;
					/*				
									if(obj->sid1 == tem_sid2 && obj->sid2 == tem_sid1){
					//do not swap if the hotness trend is kept in this checkpoint
					obj->sid1 = tem_sid1;
					obj->sid2 = tem_sid2;
					}
					else {
					//swap, the hotness trend is changed in the next checkpoint 
					obj->sid1 = tem_sid2;
					obj->sid2 = tem_sid1;
					}
					*/
				}
				else {
					//MSSD_CHECK_MODE
					//simple swap from last previous mid
					obj->sid1 = obj->ckpt_sid2;
					obj->sid2 = obj->ckpt_sid1;
					//save the suggested trims
					obj->mid_sid1 = tem_sid1;
					obj->mid_sid2 = tem_sid2;
				}
			}
		}
		else if(strstr(obj->fn, "index") != 0) {
			//(1) compute current stream suggestion based on info within this checkpoint
			//detect primary index, primary index should be seperated 
			is_pidx = (obj->gpct1 < THRESHOLD1 || obj->gpct2 < THRESHOLD1 ||
				   	(obj->gpct1 + obj->gpct2 < THRESHOLD2));
			if(is_pidx) {
				//tem_sid1 = tem_sid2 = MSSD_OTHER_SID;
				tem_sid1 = tem_sid2 = MSSD_PRIMARY_IDX_SID;
			}
			else {
				if(obj->ws1 <= idx_p1){
					tem_sid1 = MSSD_IDX_INIT_SID - 1;
				}
				else if(idx_p1 < obj->ws1 && obj->ws1 <= idx_p2){
					tem_sid1 = MSSD_IDX_INIT_SID;
				}
				else{
					tem_sid1 = MSSD_IDX_INIT_SID + 1;
				}

				if(obj->ws2 <= idx_p1){
					tem_sid2 = MSSD_IDX_INIT_SID - 1;
				}
				else if(idx_p1 < obj->ws2 && obj->ws2 <= idx_p2){
					tem_sid2 = MSSD_IDX_INIT_SID;
				}
				else{
					tem_sid2 = MSSD_IDX_INIT_SID + 1;
				}
			}
#if defined(SSDM_OP9_DEBUG)
			printf("__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, idx_min, idx_p1, idx_p2, idx_max);
			fprintf(fp, "__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f p1 %f p2 %f max %f \n", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, idx_min, idx_p1, idx_p2, idx_max);
#endif
			//(2) map stream
			//if (time_s > MSSD_RECOVER_TIME){
			if (!m->is_recovery){
				//error collection, when the prediction is wrong, increase the error count
				if (obj->sid1 != tem_sid1)
					obj->err_count++;
				if (obj->sid2 != tem_sid2)
					obj->err_count++;
				if (mode == MSSD_CKPT_MODE) {
					//simple swap from last previous checkpoint
					obj->sid1 = obj->mid_sid2;
					obj->sid2 = obj->mid_sid1;
					//save the suggested sids
					obj->ckpt_sid1 = tem_sid1;
					obj->ckpt_sid2 = tem_sid2;
					/*
					   if(obj->sid1 == tem_sid2 && obj->sid2 == tem_sid1){
					//do not swap if the hotness trend is kept in this checkpoint
					obj->sid1 = tem_sid1;
					obj->sid2 = tem_sid2;
					}
					else {
					//swap, the hotness trend is changed in the next checkpoint 
					obj->sid1 = tem_sid2;
					obj->sid2 = tem_sid1;
					}
					*/
				}
				else {
					//MSSD_CHECK_MODE
					//simple swap from last previous mid
					obj->sid1 = obj->ckpt_sid2;
					obj->sid2 = obj->ckpt_sid1;

					obj->mid_sid1 = tem_sid1;
					obj->mid_sid2 = tem_sid2;
				}
			}
			else {

				if(is_pidx) {
					obj->sid1 = tem_sid2;
					obj->sid2 = tem_sid1;
				}
			}
		}// end else if(strstr(obj->fn, "index") != 0) 
	//	if (mode == MSSD_CKPT_MODE) {
			//reset this obj's metadata for next call 
			obj->num_w1 = obj->num_w2 = 0;
			obj->off_min1 = obj->off_min2 = 100 * obj->offset;
			obj->off_max1 = obj->off_max2 = 0;
	//	}
	}//end for
	if(m->is_recovery)
		m->is_recovery = false;
}
#endif //SSDM_OP9

#if defined(SSDM_OP11) || defined(SSDM_OP11_2)
/*
 *In SSDM_OP11, general case of SSDM_OP8 that use k groups instead 3 groups ( 3 <= k <= max_files / 2 )
 collection files use k groups: from 3 to k + 2
 index files use k groups: from k + 3 to 2k + 2
 number of pivot points for each collection/index group p = k - 1
 * */
static inline void mssdmap_flexmap(MSSD_MAP *m, FILE* fp){

	int i,j;
	int tem_sid1, tem_sid2;
	MSSD_PAIR* obj;
	bool is_pidx;
	double den1, den2, local_pct1, local_pct2, global_pct1, global_pct2, wpps1, wpps2;
	struct timeval tv_tem;
	double time_s;
	double coll_min, coll_max, idx_min, idx_max;
	double coll_p[MSSD_NUM_P];
	double idx_p[MSSD_NUM_P];
	double coll_step, idx_step;
	double min_tem, max_tem;
	//double coll_p1, coll_p2, idx_p1, idx_p2;
	size_t coll_count1, coll_count2, idx_count1, idx_count2; //counts for global cpt computation
	coll_count1 = coll_count2 = idx_count1 = idx_count2 = 0;
	coll_min = idx_min = 100000;
	coll_max = idx_max = -100000;
	coll_max = -100000;
	tem_sid1 = tem_sid2 = MSSD_UNDEFINED_SID;

//compute the ckpt interval time in seconds and update
	gettimeofday(&tv_tem, NULL);
	time_s = (tv_tem.tv_sec - m->tv.tv_sec);
	if(time_s <= 0){
				printf("error! ckpt interval <= 0");	
				return;
	}
	m->tv = tv_tem;

//phase 1: compute the total write in a stream id (similar with SSDM_OP8)
		for( i = 0; i < m->size; i++){
			obj = m->data[i];
			if(strstr(obj->fn, "collection") != 0){
				coll_count1 += obj->num_w1;
				coll_count2 += obj->num_w2;

			}
			else if(strstr(obj->fn, "index") != 0) {
				idx_count1 += obj->num_w1;	
				idx_count2 += obj->num_w2;	
			}
		}
//phase 2: compute densities, write per page per second,  and written percentages (similar with SSDM_OP8)
		for( i = 0; i < m->size; i++){
			obj = m->data[i];

			den1 = (obj->off_max1 > obj->off_min1) ? ((obj->num_w1 * 4096.0) / (obj->off_max1 - obj->off_min1)) : (-1) ;
			den2 = (obj->off_max2 > obj->off_min2) ? ((obj->num_w2 * 4096.0) / (obj->off_max2 - obj->off_min2)) : (-1) ;
			if(time_s > 0){
				wpps1 = den1 / time_s;
				wpps2 = den2 / time_s;
				//Compute logarit
				obj->ws1 = log(wpps1) / log(10);
				obj->ws2 = log(wpps2) / log(10);
			}


			if(strstr(obj->fn, "collection") != 0){
				obj->gpct1 = global_pct1 = (obj->num_w1 * 1.0) / coll_count1 * 100;
				obj->gpct2 = global_pct2 = (obj->num_w2 * 1.0) / coll_count2 * 100;
				//compute min, max
				if(obj->ws1 < coll_min)
					coll_min = obj->ws1;
				if(obj->ws2 < coll_min)
					coll_min = obj->ws2;
				if(obj->ws1 > coll_max)
					coll_max = obj->ws1;
				if(obj->ws2 > coll_max)
					coll_max = obj->ws2;
			}
			else if(strstr(obj->fn, "index") != 0) {
				obj->gpct1 = global_pct1 = (obj->num_w1 * 1.0) / idx_count1 * 100;
				obj->gpct2 = global_pct2 = (obj->num_w2 * 1.0) / idx_count2 * 100;
				//compute min, max, except index files that has global percentage too small 
				if(global_pct1 + global_pct2 > THRESHOLD2) {
					if(global_pct1 > THRESHOLD1){
						if(obj->ws1 < idx_min)
							idx_min = obj->ws1;
						if(obj->ws1 > idx_max)
							idx_max = obj->ws1;
					}
					if(global_pct2 > THRESHOLD1){
						if(obj->ws2 < idx_min)
							idx_min = obj->ws2;
						if(obj->ws2 > idx_max)
							idx_max = obj->ws2;
					}
				}
			}

		}//end for
//phase 3: stream mapping based on hotness
		//compute pivot points
		/*
		coll_p1	= coll_min + ((coll_max - coll_min) * (1.0 / ALPHA) );
		coll_p2 = coll_min + ( (coll_max - coll_min) * ((ALPHA-1)*1.0/ALPHA) );
		idx_p1	= idx_min + ((idx_max - idx_min) * (1.0 / ALPHA) );
		idx_p2 = idx_min + ( (idx_max - idx_min) * ((ALPHA-1)*1.0/ALPHA) );
		*/
		//the first and last group is stricted specially 
		coll_p[0] = coll_min + ((coll_max - coll_min) * (1.0 / ALPHA) ); 
		coll_p[MSSD_NUM_P-1] = coll_min + ( (coll_max - coll_min) * ((ALPHA-1)*1.0/ALPHA) );
		idx_p[0]	= idx_min + ((idx_max - idx_min) * (1.0 / ALPHA) );
		idx_p[MSSD_NUM_P-1] = idx_min + ( (idx_max - idx_min) * ((ALPHA-1)*1.0/ALPHA) );
		
		//the remain groups are similar, MSSD_NUM_GROUP should >= 3
		assert(MSSD_NUM_GROUP >= 3);
		coll_step = (coll_p[MSSD_NUM_P-1] - coll_p[0]) * (1.0 / (MSSD_NUM_GROUP - 2));
		idx_step = (idx_p[MSSD_NUM_P-1] - idx_p[0]) * (1.0 / (MSSD_NUM_GROUP - 2));
	
		if ( MSSD_NUM_P >= 3)	{
			for( j = 1; j <= MSSD_NUM_P - 2; j++) {
				coll_p[j] = coll_p[0] + coll_step * j; 
				idx_p[j] = idx_p[0] + idx_step * j; 
			}
		}
		//mapping
		//(1): check range: we will check the range that the wsi belong to and assign the stream-id depend which range it belongs to 
		//(2): swap sid of left and right part
		for( i = 0; i < m->size; i++){
			obj = m->data[i];
			local_pct1 = (obj->num_w1 * 1.0) / (obj->num_w1 + obj->num_w2) * 100;
			local_pct2 = (obj->num_w2 * 1.0) / (obj->num_w1 + obj->num_w2) * 100;

			if(strstr(obj->fn, "collection") != 0){
				//(1) compute suggested stream id
				//two special cases that is in begin or end 
				if(obj->ws1 <= coll_p[0])
					tem_sid1 = MSSD_COLL_INIT_SID; //first sid
				else if (coll_p[MSSD_NUM_P-1] < obj->ws1)
					tem_sid1 = MSSD_COLL_INIT_SID + MSSD_NUM_P; //last sid
				else{
					for( j = 0; j <= MSSD_NUM_P - 2; j++) {
						min_tem = coll_p[j];
						max_tem = coll_p[j + 1];
						if (min_tem < obj->ws1 && obj->ws1 <= max_tem) {
							tem_sid1 = MSSD_COLL_INIT_SID + (j + 1);
							break;
						}
					}
				}

				if(obj->ws2 <= coll_p[0])
					tem_sid2 = MSSD_COLL_INIT_SID;
				else if (coll_p[MSSD_NUM_P-1] < obj->ws2)
					tem_sid2 = MSSD_COLL_INIT_SID + MSSD_NUM_P;
				else{
					for( j = 0; j <= MSSD_NUM_P - 2; j++) {
						min_tem = coll_p[j];
						max_tem = coll_p[j + 1];
						if (min_tem < obj->ws2 && obj->ws2 <= max_tem) {
							tem_sid2 = MSSD_COLL_INIT_SID + (j + 1);
							break;
						}
					}
				}
#if defined(SSDM_OP11_DEBUG)
					
					
				printf("__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f ", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, coll_min );
				for(j = 0; j < MSSD_NUM_P; j++){
					printf("p%d %f ", j, coll_p[j]);
				}
				printf(" max %f\n", coll_max);

				fprintf(fp, "__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f ", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, coll_min );
				for(j = 0; j < MSSD_NUM_P; j++){
					fprintf(fp, "p%d %f ", j, coll_p[j]);
				}
				fprintf(fp, " max %f\n", coll_max);

#endif
				//(2) stream mapping
				//if (time_s > MSSD_RECOVER_TIME){	
				if (!m->is_recovery){	
					//error collection, when the prediction is wrong, increase the error count
					if (obj->sid1 != tem_sid1)
						obj->err_count++;
					if (obj->sid2 != tem_sid2)
						obj->err_count++;
#if defined(SSDM_OP11)
					//predict the streams will be used in next checkpoint
					mssdmap_predict_stream(obj, tem_sid1, tem_sid2);
#elif defined(SSDM_OP11_2)
			        if ((obj->prev_prev_sid1 != MSSD_UNDEFINED_SID)  && 	
							(obj->prev_prev_sid2 != MSSD_UNDEFINED_SID)) {
						//learn from the previous of previous checkpoint 
						obj->sid1 = obj->prev_prev_sid1;
						obj->sid2 = obj->prev_prev_sid2;

					}
					else {
						//It is the first checkpoint, use the same way with swap method
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
					//update 
					obj->prev_prev_sid1 = obj->prev_sid1;
					obj->prev_prev_sid2 = obj->prev_sid2;
					obj->prev_sid1 = tem_sid1;
					obj->prev_sid2 = tem_sid2;
				
#endif
				} //end time_s > MSSD_RECOVER_TIME
			}
			else if(strstr(obj->fn, "index") != 0) {
				//(1) compute suggested stream id
				//detect primary index, primary index should be seperated 
				is_pidx = (obj->gpct1 < THRESHOLD1 || obj->gpct2 < THRESHOLD1 ||
							(obj->gpct1 + obj->gpct2 < THRESHOLD2));

				if(is_pidx){
					/* since primary indexes has seq IO, map it with journal stream*/
					//tem_sid1 = tem_sid2 = MSSD_OTHER_SID;
					tem_sid1 = tem_sid2 = MSSD_PRIMARY_IDX_SID;
				}
				else {
					//(1) compute suggested stream id
					//two special cases that is in begin or end 
					if(obj->ws1 <= idx_p[0])
						tem_sid1 = MSSD_IDX_INIT_SID;
					else if (idx_p[MSSD_NUM_P-1] < obj->ws1)
						tem_sid1 = MSSD_IDX_INIT_SID + MSSD_NUM_P;
					else{
						for( j = 0; j <= MSSD_NUM_P - 2; j++) {
							min_tem = idx_p[j];
							max_tem = idx_p[j + 1];
							if (min_tem < obj->ws1 && obj->ws1 <= max_tem) {
								tem_sid1 = MSSD_IDX_INIT_SID + (j + 1);
								break;
							}
						}
					}

					if(obj->ws2 <= idx_p[0])
						tem_sid2 = MSSD_IDX_INIT_SID;
					else if (idx_p[MSSD_NUM_P-1] < obj->ws2)
						tem_sid2 = MSSD_IDX_INIT_SID + MSSD_NUM_P;
					else{
						for( j = 0; j <= MSSD_NUM_P - 2; j++) {
							min_tem = idx_p[j];
							max_tem = idx_p[j + 1];
							if (min_tem < obj->ws2 && obj->ws2 <= max_tem) {
								tem_sid2 = MSSD_IDX_INIT_SID + (j + 1);
								break;
							}
						}
					}
				}

#if defined(SSDM_OP11_DEBUG)
				printf("__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f ", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, idx_min );
				for(j = 0; j < MSSD_NUM_P; j++){
					printf("p%d %f ", j, idx_p[j]);
				}
				printf( " max %f\n", idx_max);

				fprintf(fp, "__ckpt_server name %s offset %jd num_w1 %zu num_w2 %zu tem_sid1 %d tem_sid2 %d sid1 %d sid2 %d l_pct1 %f l_pct2 %f g_pct1 %f g_pct2 %f duration_s %f wpps1 %f wpps2 %f min %f ", obj->fn, obj->offset, obj->num_w1, obj->num_w2, tem_sid1, tem_sid2, obj->sid1, obj->sid2, local_pct1, local_pct2, obj->gpct1, obj->gpct2, time_s, obj->ws1, obj->ws2, idx_min );
				for(j = 0; j < MSSD_NUM_P; j++){
					fprintf(fp, "p%d %f ", j, idx_p[j]);
				}
				fprintf(fp,  " max %f\n", idx_max);
#endif
				//(2) stream mapping
				//error collection, when the prediction is wrong, increase the error count
				if (obj->sid1 != tem_sid1)
					obj->err_count++;
				if (obj->sid2 != tem_sid2)
					obj->err_count++;

				//if (time_s < MSSD_RECOVER_TIME) {
				if (m->is_recovery) {
					//This is special case, for handling early detect primary index  
					if(is_pidx) {
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
				}
				else {
#if defined(SSDM_OP11)

					//predict the streams will be used in next checkpoint 
					mssdmap_predict_stream(obj, tem_sid1, tem_sid2);

#elif defined(SSDM_OP11_2)
			        if ((obj->prev_prev_sid1 != MSSD_UNDEFINED_SID)  && 	
							(obj->prev_prev_sid2 != MSSD_UNDEFINED_SID)) {
						//learn from the previous of previous checkpoint 
						obj->sid1 = obj->prev_prev_sid1;
						obj->sid2 = obj->prev_prev_sid2;

					}
					else {
						//It is the first checkpoint, use the same way with swap method
						obj->sid1 = tem_sid2;
						obj->sid2 = tem_sid1;
					}
					//update 
					obj->prev_prev_sid1 = obj->prev_sid1;
					obj->prev_prev_sid2 = obj->prev_sid2;
					obj->prev_sid1 = tem_sid1;
					obj->prev_sid2 = tem_sid2;
#endif
				}
			} //end else index

			//update current trends
			obj->prev_tem_sid1 = tem_sid1;
			obj->prev_tem_sid2 = tem_sid2;
			//reset this obj's metadata for next ckpt
			obj->num_w1 = obj->num_w2 = 0;
			obj->off_min1 = obj->off_min2 = 100 * obj->offset;
			obj->off_max1 = obj->off_max2 = 0;
		}//end for
	//Set flag
	if(m->is_recovery) 
		m->is_recovery = false;
}
#endif //SSDM_OP11

#if defined (SSDM_OP8) || defined(SSDM_OP8_2) || defined (SSDM_OP9) || defined (SSDM_OP11)
/*
 * Report statistic information in file 
 * */
static inline void mssdmap_stat_report(MSSD_MAP* m, FILE* fp){
	int i;
	MSSD_PAIR* obj;

	for (i = 0; i < m->size; i++) {
		obj = m->data[i];
		printf(" stat_report filename %s num_err %d \n", obj->fn, obj->err_count);
		fprintf(fp, " stat_report filename %s num_err %d \n", obj->fn, obj->err_count);
	}

}
#endif //SSDM_OP8 or SSDM_OP9

#endif //__MSSD_H__
