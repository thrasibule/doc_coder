#define UP 	0
#define DOWN 	1
#define LEFT	2
#define RIGHT	3

#define UPPERLEFT  	0
#define LOWERLEFT	1

#define MAX_BOUND_POINT	10000

typedef struct boundary {
	PixelCoord coord[MAX_BOUND_POINT];
	int length;
} Boundary;

typedef struct {		/* window: a discrete 2-D rectangle */
    PixelCoord ul, lr;		/* two corners(inclusive) */
} Window;

typedef struct {
    short y, xl, xr, dy;
} Segment;

/*	 X		 X		 XX			*
 *	XOX		XOX		XOOX			*
 *	 X		XOX		 XX			*
 *			 X					*
 *								*
 *     SINGLE	      VDOUBLE	       HDOUBLE			*/
 
#define SINGLE	0
#define HDOUBLE 1
#define VDOUBLE 2
