
#include "doc_coder.h"
#include "dictionary.h"
#include "opt_dict.h"
#include <math.h>

EquiClass *equi_class;
int equi_class_num;
MarkPointerList *mark_pointer_list;
static MatchInfo *match_info;
static int class_index[MAX_MARK_NUM];

void form_equivalence_classes(void);
int  rematch_all_marks(void);
void label_marks(int *, int);
void purge_equivalence_classes(void);
float get_equi_class_repre(EquiClass *);

extern void match_word_with_dict(Word *, int *, float *);
extern void match_mark_with_dict(Mark *, int *, float *);
extern void error(char *);

extern MarkList *all_marks;
extern Dictionary *dictionary;
extern Codec *codec;
extern int prev_dict_size;

/* Subroutine: 	void form_equivalence_classes()
   Function:	group symbols into equivalence classes and decide the 
   		representatives
   Input:	none
   Output:	none
*/
void form_equivalence_classes()
{
  register int i, j;
  int ref_index;
  int new_mark_num, total_mark_num;
  Mark *mark, *mark2;
  float mm;
  
  EquiClass *cur_class;

  int not_done, iteration, old_class_num;
    
  prev_dict_size = dictionary->total_mark_num;
//  #ifdef DEBUG
  printf("prev_dict_size = %d\n", prev_dict_size);
//  #endif
  new_mark_num = all_marks->mark_num;
  total_mark_num = new_mark_num + prev_dict_size;

  #ifdef NEVER
  entry = dictionary->entries;
  for(i = 0; i < prev_dict_size; i++, entry++)
    if(entry->ref_index != -1) {
      if(entry->ref_mark != dictionary->entries[entry->ref_index].mark)
        error("form_equivalence_classes: illogical error\n");
    }
  #endif
  
  /* initialize match_info */
  match_info = (MatchInfo *)malloc(sizeof(MatchInfo)*total_mark_num);
  if(!match_info)
    error("form_equivalence_classes: Cannot allocate memory\n");
  for(i = 0; i < total_mark_num; i++) {
    match_info[i].mm_score = 1.0;
    match_info[i].ref_mark = -1;
  }
  
  /* match existing dictionary marks */
  for(i = 0; i < prev_dict_size; i++) {
    mark = dictionary->entries[i].mark;
    for(j = 0; j < i; j++) {
      mark2 = dictionary->entries[j].mark;
      if(codec->prescreen_two_marks(mark, mark2)) {
        mm = codec->match_two_marks(mark, mark2);
        if(mm <= codec->mismatch_thres) {
  	  if(mm < match_info[i].mm_score) {
            match_info[i].mm_score = mm;
            match_info[i].ref_mark = j;
          }
          if(mm < match_info[j].mm_score) {
            match_info[j].mm_score = mm;
            match_info[j].ref_mark = i;
          }      
        }
      }
    } 
  }
  
  /* match the new marks with the existing dictionary */
  mark = all_marks->marks;
  for(i = 0; i < new_mark_num; i++, mark++) {
    match_mark_with_dict(mark, &ref_index, &mm);
    match_info[i+prev_dict_size].mm_score = mm;
    match_info[i+prev_dict_size].ref_mark = ref_index;
    if(ref_index != -1 && mm < match_info[ref_index].mm_score) {
      match_info[ref_index].mm_score = mm;
      match_info[ref_index].ref_mark = i+prev_dict_size;
    }
  }

  /* match each new mark with other new marks */
  mark = all_marks->marks + 1;
  for(i = 1; i < new_mark_num; i++, mark++) {
    for(j = 0, mark2 = all_marks->marks; j < i; j++, mark2++) {
      if(codec->prescreen_two_marks(mark, mark2)) {
        mm = codec->match_two_marks(mark, mark2);
	if(mm <= codec->mismatch_thres) {
          if(mm < match_info[i+prev_dict_size].mm_score) {
            match_info[i+prev_dict_size].mm_score = mm;
	    match_info[i+prev_dict_size].ref_mark = j+prev_dict_size;
          }
          if(mm < match_info[j+prev_dict_size].mm_score) {
            match_info[j+prev_dict_size].mm_score = mm;
	    match_info[j+prev_dict_size].ref_mark = i+prev_dict_size;
          }
        }
      }
    }
  }
  
  /* assign equal labels to each small connected graph (equivalence class) */
  label_marks(class_index, total_mark_num);
  free((void *)match_info);

  /* allocate memory for equivalence classes */
  if(equi_class_num > MAX_EQUI_CLASS_NUM)
    error("form_equivalence_classes: equi_class buffer is full\n");
  equi_class = (EquiClass *)malloc(sizeof(EquiClass)*MAX_EQUI_CLASS_NUM);
  if(!equi_class)
    error("form_equivalence_classes: Cannot allocate memory\n");
      
  /* group marks with the same label into one equivalence class */
  for(i = 0, cur_class = equi_class; i < equi_class_num; i++, cur_class++)
    cur_class->total_entry_num = 0;
  
  for(i = 0; i < total_mark_num; i++) {
    cur_class = equi_class + class_index[i];
    cur_class->entries[cur_class->total_entry_num++] = i;
    if(cur_class->total_entry_num == MAX_EQUI_CLASS)
      error("form_equivalence_classes: class buffer is full!\n");
  }
  
  /* remove the equivalence classes which contain only existing dict marks */
  purge_equivalence_classes();
  
  /* decide the representive for each equivalence class */
  for(i = 0, cur_class = equi_class; i < equi_class_num; i++, cur_class++) 
    get_equi_class_repre(cur_class);

  /* in the LLOYD mode, rematch all marks and recalculate all 
     representatives until nothing changes anymore */
  if(codec->lloyd) {
    old_class_num = equi_class_num;
    iteration = 0; not_done = TRUE;
    while(not_done) {
      not_done = rematch_all_marks();
      iteration++;
    }
    if(!codec->silent) {
      printf("INFO: %d iterations before the LLOYD algorithm converges\n",
        iteration);
      if(old_class_num < equi_class_num)
        printf("INFO: %d new equivalence classes are generated by LLOYD\n", 
               equi_class_num - old_class_num);
    }
  }
} 

int is_usable_label(int, int *, int);

/* Algorithm in pseudo-code:
	read currect entry;
	if doesn't have a label {
	  read reference entry;
	  if reference exist {
	    if reference has label, use reference label;
	    else {
	      if "usable label list" is empty, assign a new label;
	      else get a label from list;
	      assign current entry and its reference this label;
	    }
	  }
	  else {
	      if "usable label list" is empty, assign a new label;
	      else get a label from list;
	      assign current entry this label;
	  }
	}
	else {
	  read reference;
	  if reference exist {
	    if reference doesn't have label, assign its label to reference;
	    else if reference has a different label {
	      pick one label out of the two;
	      change every entry with the other label to this label;
	      put the other label in "usable label list";
	    }
	  }
	}
*/
void label_marks(int *equi_class_label, int total_mark_num)
{
  register int cur, i;
  int *usable_labels;	/* stack for usable labels */
  int usable_label_num;
  int ref, cur_label;
  int one, the_other;
  
  usable_labels = (int *)malloc(sizeof(int)*total_mark_num);
  if(!usable_labels) 
    error("label_dictionary: cannot allocate memory\n");
    
  /* class label initialization */
  for(cur = 0; cur < total_mark_num; cur++) 
    equi_class_label[cur] = -1;
  
  equi_class_num = 0; usable_label_num = 0;
  for(cur = 0; cur < total_mark_num; cur++) {
    ref = match_info[cur].ref_mark;
    if(equi_class_label[cur] == -1) {
      if(ref != -1) {
        if(equi_class_label[ref] == -1) {
      	  if(usable_label_num > 0) 
            cur_label = usable_labels[--usable_label_num];
          else cur_label = equi_class_num++;
          equi_class_label[cur] = equi_class_label[ref] = cur_label;
        }
	else equi_class_label[cur] = equi_class_label[ref];
      }
      else {
  	if(usable_label_num > 0) 
          cur_label = usable_labels[--usable_label_num];
        else cur_label = equi_class_num++;
        equi_class_label[cur] = cur_label;
      }
    }
    else {
      if(ref != -1) {
        if(equi_class_label[ref] == -1)
	  equi_class_label[ref] = equi_class_label[cur];
        else {
	  if(equi_class_label[ref] != equi_class_label[cur]) {
	    one = equi_class_label[cur];
	    the_other = equi_class_label[ref];

	    for(i = 0; i < total_mark_num; i++) 
	      if(equi_class_label[i] == one)
	      	equi_class_label[i] = the_other;

	    usable_labels[usable_label_num++] = one;
	  }
	}
      }
    }
  }
  
  /* clean up the usable label stack */
  while(usable_label_num > 0) {
    one = usable_labels[--usable_label_num];
    if(one < equi_class_num)	{
      the_other = --equi_class_num;
      while(is_usable_label(the_other, usable_labels, usable_label_num))
        the_other = --equi_class_num;
      for(cur = 0; cur < total_mark_num; cur++) 
        if(equi_class_label[cur] == the_other) 
          equi_class_label[cur] = one;
    }
  }
  
  free((void *)usable_labels);
}

/* Subroutine:	int is_usable_label()
   Function:	decide if the input label "l" is inside the usable label 
   		buffer "uls"
   Input:	input label, usable label buffer and its size
   Output:	binary decision
*/
int is_usable_label(int l, int *uls, int ul_num) 
{
  register int i;
  
  for(i = 0; i < ul_num; i++)
    if(l == uls[i]) return TRUE;
  return FALSE;
}

int unnecessary_class(EquiClass *);
void delete_equi_classes(int *, int);

/* Subroutine:	void purge_equivalence_classes()
   Function:	examine the classes formed and delete those that contain
   		only symbols from the existing dictionary
   Input:	none
   Output:	none
*/
void purge_equivalence_classes()
{
  register int i;
  int *empty_classes, empty_class_num;
  
  /* if there's no previous dictionary yet, return right away */
  if(prev_dict_size == 0) return;
  
  empty_classes = (int *)malloc(sizeof(int)*equi_class_num);
  if(!empty_classes)
    error("purge_equivalence_classes: Cannot allocate memory\n");
    
  for(i = 0, empty_class_num = 0; i < equi_class_num; i++) {
    if(unnecessary_class(equi_class+i)) 
      empty_classes[empty_class_num++] = i;
  }
  
  delete_equi_classes(empty_classes, empty_class_num);
  free((void *)empty_classes);
}

/* Subroutine:	int unnecessary_class()
   Function:	decide if this class need not exist because it contains
   		only existing dictionary marks
   Input:	class to be examined
   Output:	binary decision
*/
int unnecessary_class(EquiClass *cur_class)
{
  register int i;
  
  for(i = 0; i < cur_class->total_entry_num; i++)
    if(cur_class->entries[i] >= prev_dict_size) break;

  if(i < cur_class->total_entry_num) return FALSE;
  else return TRUE;
}

void copy_equi_class(EquiClass *, EquiClass *);

/* Subroutine:	void delete_equi_classes()
   Function:	delete the equivalence classes designated by the input buffer
   Input:	buffer of classes to be deleted and its size
   Output:	none
*/
void delete_equi_classes(int *classes, int class_num)
{
  register int i, j;
  
  for(i = 0; i < class_num-1; i++) {
    for(j = classes[i]+1; j < classes[i+1]; j++) 
      copy_equi_class(equi_class + j, equi_class + (j-i-1));
  }
  
  for(j = classes[class_num-1]+1; j < equi_class_num; j++) 
    copy_equi_class(equi_class + j, equi_class + (j-class_num));
   
  equi_class_num -= class_num;
}

/* Subroutine:	void copy_equi_class()
   Function:	copy the content of the class "src" into "dest"
   Input:	source and destination classes
   Output:	none
*/
void copy_equi_class(EquiClass *src, EquiClass *dest)
{
  register int i;
  
  for(i = 0; i < src->total_entry_num; i++)
    dest->entries[i] = src->entries[i];
    
  dest->total_entry_num = src->total_entry_num;
  dest->repre = src->repre;
}

/* Subroutine:	void get_equi_class_repre()
   Function:	decide the representative for the input equivalence class as 
   		the entry with the smallest average mismatch
   Input:	the class
   Output:	the average mismatch
*/
float get_equi_class_repre(EquiClass *cur_class)
{
  register int i, j;
  int entry_num, eff_entry_num;
  float lowest_ave_mm, cur_ave_mm;

  Mark *cur, *match;
  float *mm;
  
  entry_num = cur_class->total_entry_num;
  
  /* for class with only 1 member, its representative is this only member */
  if(entry_num == 1) {
    cur_class->repre = cur_class->entries[0];
    return 0.;
  }
  
  /* for class with only 2 members, we take the member with smaller index 
     as the representative. this way we make sure we always choose an
     existing dictionary mark as the representative if possible */
  else if(entry_num == 2) {
    if(cur_class->entries[0] < cur_class->entries[1]) 
      cur_class->repre = cur_class->entries[0];
    else cur_class->repre = cur_class->entries[1];
    
    if(cur_class->entries[0] < prev_dict_size)
      cur = dictionary->entries[cur_class->entries[0]].mark;
    else cur = all_marks->marks + (cur_class->entries[0]-prev_dict_size);
    if(cur_class->entries[1] < prev_dict_size)
      match = dictionary->entries[cur_class->entries[1]].mark;
    else match = all_marks->marks + (cur_class->entries[1]-prev_dict_size);
    return codec->match_two_marks(cur, match);
  }
  
  /* for class with more than 3 members, calculate the average mismatch */
  mm = (float *)malloc(sizeof(float)*entry_num*entry_num);
  if(!mm) error("get_equi_class_repre: Cannot allocate memory\n");

  memset(mm, 0, sizeof(float)*entry_num*entry_num);
  
  /* match all class members pair-wise */
  for(i = 0; i < entry_num; i++) {
    if(cur_class->entries[i] < prev_dict_size) 
      cur = dictionary->entries[cur_class->entries[i]].mark;
    else cur = all_marks->marks + (cur_class->entries[i]-prev_dict_size);
    for(j = 0; j < i; j++) {
      if(cur_class->entries[j] < prev_dict_size) 
        match = dictionary->entries[cur_class->entries[j]].mark;
      else match = all_marks->marks + (cur_class->entries[j]-prev_dict_size);
      
      /* we don't match two existing dictionary marks */
      if(cur_class->entries[i] >= prev_dict_size ||
         cur_class->entries[j] >= prev_dict_size) {
        mm[i*entry_num+j] = codec->match_two_marks(cur, match);
        mm[j*entry_num+i] = mm[i*entry_num+j];
      }
    }
  }
  
  lowest_ave_mm = 1.;
  for(i = 0; i < entry_num; i++) {
    /* we match only the current entry with other entries representing 
       new marks, pre-existing dictionary marks don't count into average
       mismatch */
    cur_ave_mm = 0.; eff_entry_num = 0;
    for(j = 0; j < entry_num; j++) {
      if(i != j && cur_class->entries[j] >= prev_dict_size) {
        cur_ave_mm += mm[i*entry_num+j];
	eff_entry_num++;
      }
    }
    cur_ave_mm /= (float)(eff_entry_num);
    
    if(cur_ave_mm < lowest_ave_mm) {
      cur_class->repre = cur_class->entries[i];
      lowest_ave_mm = cur_ave_mm;
    }
  }
  
  free((void *)mm);
  return lowest_ave_mm;
}

int find_closest_repre(Mark *);
void remove_class_member(EquiClass *, int);
void add_class_member(EquiClass *, int);
void add_new_class(int);

/* Subroutine:	int rematch_all_marks()
   Function:	rematch all the marks with representatives and recalculate
   		the representatives according to new matching results
   Input:	none
   Output:	whether the algorithm has converged
*/
int rematch_all_marks()
{
  register int i;
  int new_class;
  int members_changed[MAX_MARK_NUM];
  int classes_modified;
  Mark *mark;
  
  for(i = 0; i < equi_class_num; i++)
    members_changed[i] = FALSE;
    
  for(i = 0, mark = all_marks->marks; i < all_marks->mark_num; i++, mark++) {
    if((equi_class[class_index[i]].repre != i) &&
       (equi_class[class_index[i]].total_entry_num > 2)) {
      new_class = find_closest_repre(mark);
      /* if the mark can't be matched, add a new equi_class to buffer it */
      if(new_class == -1) {
        remove_class_member(equi_class + class_index[i], i);
	add_new_class(i);
      }
      else if(new_class != class_index[i]) {
        remove_class_member(equi_class + class_index[i], i);
	add_class_member(equi_class + new_class, i); 
	class_index[i] = new_class;
	members_changed[class_index[i]] = TRUE;
        members_changed[new_class] = TRUE;
      }
    }
  }
  
  classes_modified = FALSE;
  for(i = 0; i < equi_class_num; i++) 
    if(members_changed[i]) {
      classes_modified = TRUE;
      get_equi_class_repre(equi_class + i);
    }
  
  return classes_modified;
}

/* Subroutine:	int find_closest_repre()
   Function:	for the input mark find the representative that's closest to it
   Input:	the mark
   Output:	the representative's index
*/
int find_closest_repre(Mark *mark)
{
  register int i;
  float cur, lowest;
  int closest;
  Mark *repre;
  
  lowest = 1.0; closest = -1;
  for(i = 0; i < equi_class_num; i++) {
    repre = all_marks->marks + equi_class[i].repre;
    if(codec->prescreen_two_marks(repre, mark)) {
      cur = codec->match_two_marks(repre, mark); 
      if(cur < lowest) {
        lowest = cur;
        closest = i;
      }
    }
  }
  
  return closest;  
}

/* Subroutine:	void remove_class_member()
   Function:	remove a member from the input class
   Input:	the class and the member to be removed
   Output:	none
*/
void remove_class_member(EquiClass *cur_class, int member)
{
  register int i, j;
  
  for(i = 0; i < cur_class->total_entry_num; i++) 
    if(cur_class->entries[i] == member) break;
  
  if(i == cur_class->total_entry_num)
    error("remove_class_member: illogical error, can't find member in class\n");
  
  for(j = i; j < cur_class->total_entry_num; j++)
    cur_class->entries[j] = cur_class->entries[j+1];
  cur_class->total_entry_num--;
}

/* Subroutine:	void add_class_member()
   Function:	add a member to the input class
   Input:	the class and the new member to be added
   Output:	none
*/
void add_class_member(EquiClass *cur_class, int member)
{
  if(cur_class->total_entry_num == MAX_EQUI_CLASS) 
    error("add_class_member: class buffer is full!\n");
  
  cur_class->entries[cur_class->total_entry_num++] = member;
}

/* Subroutine:	void add_new_class()
   Function:	add a new equivalence class
   Input:	the member in the new class
   Output:	none
*/
void add_new_class(int member)
{
  EquiClass *cur_class;
  
  if(equi_class_num == MAX_EQUI_CLASS_NUM) 
    error("add_new_class: buffer equi_class is full\n");
    
  cur_class = equi_class + equi_class_num;
  cur_class->total_entry_num = 1;
  cur_class->entries[0] = cur_class->repre = member;
  
  equi_class_num++;
}

