
CC = gcc

CCFLAGS = -O1

# -DDEBUG 
# -DDICTIONARY

CODEROBJ = main.o encode.o dictionary.o opt_dict_mixed.o \
	   opt_dict_class.o opt_dict_tree.o equi_class.o mark.o match.o \
	   symbol_region.o generic_region.o read_write.o decode.o \
	   entropy.o mq.o mq_tbl.o huff_tbl.o misc.o 


doc_coder:	$(CODEROBJ)
		$(CC) $(CCFLAGS) -o doc_coder $(CODEROBJ) -lm


clean:		
		/bin/rm -f $(CODEROBJ) doc_coder


# dependencies for header files
main.o:		main.c doc_coder.h entropy.h mq.h Makefile
		$(CC) $(CCFLAGS) -c main.c
encode.o:	encode.c doc_coder.h encode.h dictionary.h entropy.h mq.h Makefile
		$(CC) $(CCFLAGS) -c encode.c
dictionary.o:	dictionary.c doc_coder.h dictionary.h opt_dict.h entropy.h mq.h Makefile
		$(CC) $(CCFLAGS) -c dictionary.c
opt_dict_mixed.o:opt_dict_mixed.c doc_coder.h dictionary.h opt_dict.h Makefile
		$(CC) $(CCFLAGS) -c opt_dict_mixed.c
opt_dict_tree.o:opt_dict_tree.c doc_coder.h dictionary.h opt_dict.h Makefile
		$(CC) $(CCFLAGS) -c opt_dict_tree.c
opt_dict_class.o:opt_dict_class.c doc_coder.h dictionary.h opt_dict.h Makefile
		$(CC) $(CCFLAGS) -c opt_dict_class.c
equi_class.o:   equi_class.c doc_coder.h dictionary.h opt_dict.h Makefile
		$(CC) $(CCFLAGS) -c equi_class.c
symbol_region.o:symbol_region.c doc_coder.h dictionary.h entropy.h mq.h Makefile
		$(CC) $(CCFLAGS) -c symbol_region.c
generic_region.o:generic_region.c doc_coder.h dictionary.h entropy.h mq.h Makefile
		$(CC) $(CCFLAGS) -c generic_region.c
mark.o:		mark.c doc_coder.h mark.h Makefile
		$(CC) $(CCFLAGS) -c mark.c
match.o:	match.c doc_coder.h dictionary.h Makefile
		$(CC) $(CCFLAGS) -c match.c
entropy.o:	entropy.c doc_coder.h entropy.h dictionary.h Makefile
		$(CC) $(CCFLAGS) -c entropy.c
mq.o:		mq.c doc_coder.h entropy.h mq.h Makefile
		$(CC) $(CCFLAGS) -c mq.c
mq_tbl.o:	mq_tbl.c Makefile
		$(CC) $(CCFLAGS) -c mq_tbl.c
huff_tbl.o:	huff_tbl.c entropy.h Makefile
		$(CC) $(CCFLAGS) -c huff_tbl.c
decode.o:	decode.c doc_coder.h Makefile
		$(CC) $(CCFLAGS) -c decode.c
read_write.o:	read_write.c dictionary.h doc_coder.h Makefile
		$(CC) $(CCFLAGS) -c read_write.c
misc.o:		misc.c doc_coder.h Makefile
		$(CC) $(CCFLAGS) -c misc.c

