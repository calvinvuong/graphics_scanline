/*========== my_main.c ==========

  This is the only file you need to modify in order
  to get a working mdl project (for now).

  my_main.c will serve as the interpreter for mdl.
  When an mdl script goes through a lexer and parser, 
  the resulting operations will be in the array op[].

  Your job is to go through each entry in op and perform
  the required action from the list below:

  push: push a new origin matrix onto the origin stack
  pop: remove the top matrix on the origin stack

  move/scale/rotate: create a transformation matrix 
  based on the provided values, then 
  multiply the current top of the
  origins stack by it.

  box/sphere/torus: create a solid object based on the
  provided values. Store that in a 
  temporary matrix, multiply it by the
  current top of the origins stack, then
  call draw_polygons.

  line: create a line based on the provided values. Store 
  that in a temporary matrix, multiply it by the
  current top of the origins stack, then call draw_lines.

  save: call save_extension with the provided filename

  display: view the image live
  
  jdyrlandweaver
  =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "y.tab.h"

#include "matrix.h"
#include "ml6.h"
#include "display.h"
#include "draw.h"
#include "stack.h"

/*======== void first_pass() ==========
  Inputs:   
  Returns: 

  Checks the op array for any animation commands
  (frames, basename, vary)
  
  Should set num_frames and basename if the frames 
  or basename commands are present

  If vary is found, but frames is not, the entire
  program should exit.

  If frames is found, but basename is not, set name
  to some default value, and print out a message
  with the name being used.

  jdyrlandweaver
  ====================*/
void first_pass() {
  //in order to use name and num_frames
  //they must be extern variables
  extern int num_frames;
  extern char name[128];

  int frame_found = 0; // 0 if frame command absent, 1 if present
  int basename_found = 0; // 0 if basename command absent, 1 if present
  int vary_found = 0; // 0 if vary command absent, 1 if present
  
  int i;
  for ( i = 0; i < lastop; i++ ) {
    if ( op[i].opcode == FRAMES ) {
      num_frames = op[i].op.frames.num_frames;
      frame_found += 1;
    }
    else if ( op[i].opcode == BASENAME ) {
      strcpy(name, op[i].op.basename.p->name);
      basename_found += 1;
    }
    else if ( op[i].opcode == VARY ) {
      vary_found += 1;
    }
  }

  if ( vary_found && !frame_found ) {
    printf("You are varying a value without any frames. Exiting program...\n");
    exit(0);
  }
  else if ( frame_found && !basename_found ) {
    strcpy(name, "animation");
    printf("Basename not defined. Set to 'animation' as default.\n");
  }
  return;
}

/*======== struct vary_node ** second_pass() ==========
  Inputs:   
  Returns: An array of vary_node linked lists

  In order to set the knobs for animation, we need to keep
  a separate value for each knob for each frame. We can do
  this by using an array of linked lists. Each array index
  will correspond to a frame (eg. knobs[0] would be the first
  frame, knobs[2] would be the 3rd frame and so on).

  Each index should contain a linked list of vary_nodes, each
  node contains a knob name, a value, and a pointer to the
  next node.

  Go through the opcode array, and when you find vary, go 
  from knobs[0] to knobs[frames-1] and add (or modify) the
  vary_node corresponding to the given knob with the
  appropirate value. 

  jdyrlandweaver
  ====================*/
struct vary_node ** second_pass() {
  struct vary_node ** array = malloc(sizeof(struct vary_node *) * num_frames);
  
  int frame, i;
  for ( frame = 0; frame < num_frames; frame++ ) {
    struct vary_node * top_node = NULL;
    for ( i = 0; i < lastop; i++ ) {
      if ( op[i].opcode == VARY &&
	   op[i].op.vary.start_frame <= frame &&
	   op[i].op.vary.end_frame >= frame ) {

	int start_frame = op[i].op.vary.start_frame;
	int end_frame = op[i].op.vary.end_frame;
	float percent = (frame - start_frame) / (double) (end_frame - start_frame); // percent of the transformation completed at this frame

	struct vary_node * node = (struct vary_node *) malloc(sizeof(struct vary_node));
	
	strcpy(node->name, op[i].op.vary.p->name);
	node->value = op[i].op.vary.start_val + percent * (op[i].op.vary.end_val - op[i].op.vary.start_val);
	node->next = top_node;
	  
	top_node = node;
      }
    } // end loop through operations

    array[frame] = top_node;
  } //end loop through num_frames
  //return array;
  return array;
}


/*======== void print_knobs() ==========
  Inputs:   
  Returns: 

  Goes through symtab and display all the knobs and their
  currnt values

  jdyrlandweaver
  ====================*/
void print_knobs() {
  
  int i;

  printf( "ID\tNAME\t\tTYPE\t\tVALUE\n" );
  for ( i=0; i < lastsym; i++ ) {

    if ( symtab[i].type == SYM_VALUE ) {
      printf( "%d\t%s\t\t", i, symtab[i].name );

      printf( "SYM_VALUE\t");
      printf( "%6.2f\n", symtab[i].s.value);
    }
  }
}


/*======== void my_main() ==========
  Inputs: 
  Returns: 

  This is the main engine of the interpreter, it should
  handle most of the commadns in mdl.

  If frames is not present in the source (and therefore 
  num_frames is 1, then process_knobs should be called.

  If frames is present, the enitre op array must be
  applied frames time. At the end of each frame iteration
  save the current screen to a file named the
  provided basename plus a numeric string such that the
  files will be listed in order, then clear the screen and
  reset any other data structures that need it.

  Important note: you cannot just name your files in 
  regular sequence, like pic0, pic1, pic2, pic3... if that
  is done, then pic1, pic10, pic11... will come before pic2
  and so on. In order to keep things clear, add leading 0s
  to the numeric portion of the name. If you use sprintf, 
  you can use "%0xd" for this purpose. It will add at most
  x 0s in front of a number, if needed, so if used correctly,
  and x = 4, you would get numbers like 0001, 0002, 0011,
  0487

  jdyrlandweaver
  ====================*/
void my_main() {

  int i, j;
  struct matrix *tmp;
  struct stack *systems;
  screen t;
  color g;
  double step = 0.1;
  double theta;
  
  systems = new_stack();
  tmp = new_matrix(4, 1000);
  clear_screen( t );
  g.red = 0;
  g.green = 0;
  g.blue = 0;

  first_pass();
  
  struct vary_node ** array = second_pass();

  if ( num_frames > 1 ) {
    for ( i = 0; i < num_frames; i++ ) {

      // go through linked list and set values in symbol table
      struct vary_node * top = array[i];
      while ( top != NULL ) {
	set_value(lookup_symbol(top->name), top->value);
	top = top->next;
      } // close loop thru LL
      print_knobs();

      // perform execution; loop through command list
      for (j=0;j<lastop;j++) {
	double knob_value = 1.0;
	
	switch (op[j].opcode)
	  {
	  case SPHERE:
	    printf("Sphere: %6.2f %6.2f %6.2f r=%6.2f",
		   op[j].op.sphere.d[0],op[j].op.sphere.d[1],
		   op[j].op.sphere.d[2],
		   op[j].op.sphere.r);
	    if (op[j].op.sphere.constants != NULL)
	      {
		printf("\tconstants: %s",op[j].op.sphere.constants->name);
	      }
	    if (op[j].op.sphere.cs != NULL)
	      {
		printf("\tcs: %s",op[j].op.sphere.cs->name);
	      }
	    add_sphere(tmp, op[j].op.sphere.d[0],
		       op[j].op.sphere.d[1],
		       op[j].op.sphere.d[2],
		       op[j].op.sphere.r, step);
	    matrix_mult( peek(systems), tmp );
	    draw_polygons(tmp, t, g);
	    tmp->lastcol = 0;
	    break;
	  case TORUS:
	    printf("Torus: %6.2f %6.2f %6.2f r0=%6.2f r1=%6.2f",
		   op[j].op.torus.d[0],op[j].op.torus.d[1],
		   op[j].op.torus.d[2],
		   op[j].op.torus.r0,op[j].op.torus.r1);
	    if (op[j].op.torus.constants != NULL)
	      {
		printf("\tconstants: %s",op[j].op.torus.constants->name);
	      }
	    if (op[j].op.torus.cs != NULL)
	      {
		printf("\tcs: %s",op[j].op.torus.cs->name);
	      }
	    add_torus(tmp,
		      op[j].op.torus.d[0],
		      op[j].op.torus.d[1],
		      op[j].op.torus.d[2],
		      op[j].op.torus.r0,op[j].op.torus.r1, step);
	    matrix_mult( peek(systems), tmp );
	    draw_polygons(tmp, t, g);
	    tmp->lastcol = 0;	  
	    break;
	  case BOX:
	    printf("Box: d0: %6.2f %6.2f %6.2f d1: %6.2f %6.2f %6.2f",
		   op[j].op.box.d0[0],op[j].op.box.d0[1],
		   op[j].op.box.d0[2],
		   op[j].op.box.d1[0],op[j].op.box.d1[1],
		   op[j].op.box.d1[2]);
	    if (op[j].op.box.constants != NULL)
	      {
		printf("\tconstants: %s",op[j].op.box.constants->name);
	      }
	    if (op[j].op.box.cs != NULL)
	      {
		printf("\tcs: %s",op[j].op.box.cs->name);
	      }
	    add_box(tmp,
		    op[j].op.box.d0[0],op[j].op.box.d0[1],
		    op[j].op.box.d0[2],
		    op[j].op.box.d1[0],op[j].op.box.d1[1],
		    op[j].op.box.d1[2]);
	    matrix_mult( peek(systems), tmp );
	    draw_polygons(tmp, t, g);
	    tmp->lastcol = 0;
	    break;
	  case LINE:
	    printf("Line: from: %6.2f %6.2f %6.2f to: %6.2f %6.2f %6.2f",
		   op[j].op.line.p0[0],op[j].op.line.p0[1],
		   op[j].op.line.p0[1],
		   op[j].op.line.p1[0],op[j].op.line.p1[1],
		   op[j].op.line.p1[1]);
	    if (op[j].op.line.constants != NULL)
	      {
		printf("\n\tConstants: %s",op[j].op.line.constants->name);
	      }
	    if (op[j].op.line.cs0 != NULL)
	      {
		printf("\n\tCS0: %s",op[j].op.line.cs0->name);
	      }
	    if (op[j].op.line.cs1 != NULL)
	      {
		printf("\n\tCS1: %s",op[j].op.line.cs1->name);
	      }
	    break;
	  case MOVE:

	    printf("Move: %6.2f %6.2f %6.2f",
		   op[j].op.move.d[0],op[j].op.move.d[1],
		   op[j].op.move.d[2]);
	    if (op[j].op.move.p != NULL)
	      {
		printf("\tknob: %s",op[j].op.move.p->name);
	      }

	    if ( op[j].op.move.p != NULL ) // if knob present
	      knob_value = lookup_symbol(op[j].op.move.p->name)->s.value;
	    
	    tmp = make_translate( op[j].op.move.d[0] * knob_value,
				  op[j].op.move.d[1] * knob_value,
				  op[j].op.move.d[2] * knob_value );

	    matrix_mult(peek(systems), tmp);
	    copy_matrix(tmp, peek(systems));
	    tmp->lastcol = 0;
	    break;
	  case ROTATE:
	    printf("Rotate: axis: %6.2f degrees: %6.2f",
		   op[j].op.rotate.axis,
		   op[j].op.rotate.degrees);
	    if (op[j].op.rotate.p != NULL)
	      {
		printf("\tknob: %s",op[j].op.rotate.p->name);
	      }

	    if ( op[j].op.rotate.p != NULL ) 
	      knob_value = lookup_symbol(op[j].op.rotate.p->name)->s.value;

	    theta =  op[j].op.rotate.degrees * (M_PI / 180);
	    if (op[j].op.rotate.axis == 0 )
	      tmp = make_rotX( theta * knob_value );
	    else if (op[j].op.rotate.axis == 1 )
	      tmp = make_rotY( theta * knob_value );
	    else
	      tmp = make_rotZ( theta * knob_value );
	    	    
	    matrix_mult(peek(systems), tmp);
	    copy_matrix(tmp, peek(systems));
	    tmp->lastcol = 0;
	    break;
	  case SCALE:
	    printf("Scale: %6.2f %6.2f %6.2f",
		   op[j].op.scale.d[0],op[j].op.scale.d[1],
		   op[j].op.scale.d[2]);
	    if (op[j].op.scale.p != NULL)
	      {
		printf("\tknob: %s",op[j].op.scale.p->name);
	      }

	    if ( op[j].op.scale.p != NULL )
	      knob_value = lookup_symbol(op[j].op.scale.p->name)->s.value;

	    tmp = make_scale( op[j].op.scale.d[0] * knob_value,
			      op[j].op.scale.d[1] * knob_value,
			      op[j].op.scale.d[2] * knob_value);
	    
	    matrix_mult(peek(systems), tmp);
	    copy_matrix(tmp, peek(systems));
	    tmp->lastcol = 0;
	    break;
	  case SET:
	    printf("Set");
	    set_value(lookup_symbol(op[j].op.set.p->name), op[j].op.set.p->s.value);
	    break;
	  case SETKNOBS:
	    printf("Set Knobs");
	    int k;
	    // loop through symtab and set values
	    for ( k=0; k < lastsym; k++ ) {
	      if ( symtab[i].type == SYM_VALUE )
		symtab[i].s.value = op[j].op.setknobs.value;
	    }
	    break;
	  case PUSH:
	    printf("Push");
	    push(systems);
	    break;
	  case POP:
	    printf("Pop");
	    pop(systems);
	    break;
	  case SAVE:
	    printf("Save: %s",op[i].op.save.p->name);
	    save_extension(t, op[i].op.save.p->name);
	    break;
	  case DISPLAY:
	    printf("Display");
	    display(t);
	    break;
	  }
	printf("\n");
      } // close loop thru operations
      // save image
      char dir_name[300];
      sprintf(dir_name, "anim/%s%03d", name, i);
      save_extension(t, dir_name);
      printf("Frame #%d saved as %s\n", i, dir_name);
      // reset
      tmp->lastcol = 0;
      systems = new_stack();
      clear_screen(t);
    } // close loop thru frames
  } // close if num_frames > 1

  // IF NOT ANIMATION
  else{ 
    for (i=0;i<lastop;i++) {
      
      printf("%d: ",i);
      switch (op[i].opcode)
	{
	case SPHERE:
	  printf("Sphere: %6.2f %6.2f %6.2f r=%6.2f",
		 op[i].op.sphere.d[0],op[i].op.sphere.d[1],
		 op[i].op.sphere.d[2],
		 op[i].op.sphere.r);
	  if (op[i].op.sphere.constants != NULL)
	    {
	      printf("\tconstants: %s",op[i].op.sphere.constants->name);
	    }
	  if (op[i].op.sphere.cs != NULL)
	    {
	      printf("\tcs: %s",op[i].op.sphere.cs->name);
	    }
	  add_sphere(tmp, op[i].op.sphere.d[0],
		     op[i].op.sphere.d[1],
		     op[i].op.sphere.d[2],
		     op[i].op.sphere.r, step);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, g);
	  tmp->lastcol = 0;
	  break;
	case TORUS:
	  printf("Torus: %6.2f %6.2f %6.2f r0=%6.2f r1=%6.2f",
		 op[i].op.torus.d[0],op[i].op.torus.d[1],
		 op[i].op.torus.d[2],
		 op[i].op.torus.r0,op[i].op.torus.r1);
	  if (op[i].op.torus.constants != NULL)
	    {
	      printf("\tconstants: %s",op[i].op.torus.constants->name);
	    }
	  if (op[i].op.torus.cs != NULL)
	    {
	      printf("\tcs: %s",op[i].op.torus.cs->name);
	    }
	  add_torus(tmp,
		    op[i].op.torus.d[0],
		    op[i].op.torus.d[1],
		    op[i].op.torus.d[2],
		    op[i].op.torus.r0,op[i].op.torus.r1, step);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, g);
	  tmp->lastcol = 0;	  
	  break;
	case BOX:
	  printf("Box: d0: %6.2f %6.2f %6.2f d1: %6.2f %6.2f %6.2f",
		 op[i].op.box.d0[0],op[i].op.box.d0[1],
		 op[i].op.box.d0[2],
		 op[i].op.box.d1[0],op[i].op.box.d1[1],
		 op[i].op.box.d1[2]);
	  if (op[i].op.box.constants != NULL)
	    {
	      printf("\tconstants: %s",op[i].op.box.constants->name);
	    }
	  if (op[i].op.box.cs != NULL)
	    {
	      printf("\tcs: %s",op[i].op.box.cs->name);
	    }
	  add_box(tmp,
		  op[i].op.box.d0[0],op[i].op.box.d0[1],
		  op[i].op.box.d0[2],
		  op[i].op.box.d1[0],op[i].op.box.d1[1],
		  op[i].op.box.d1[2]);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, g);
	  tmp->lastcol = 0;
	  break;
	case LINE:
	  printf("Line: from: %6.2f %6.2f %6.2f to: %6.2f %6.2f %6.2f",
		 op[i].op.line.p0[0],op[i].op.line.p0[1],
		 op[i].op.line.p0[1],
		 op[i].op.line.p1[0],op[i].op.line.p1[1],
		 op[i].op.line.p1[1]);
	  if (op[i].op.line.constants != NULL)
	    {
	      printf("\n\tConstants: %s",op[i].op.line.constants->name);
	    }
	  if (op[i].op.line.cs0 != NULL)
	    {
	      printf("\n\tCS0: %s",op[i].op.line.cs0->name);
	    }
	  if (op[i].op.line.cs1 != NULL)
	    {
	      printf("\n\tCS1: %s",op[i].op.line.cs1->name);
	    }
	  break;
	case MOVE:
	  printf("Move: %6.2f %6.2f %6.2f",
		 op[i].op.move.d[0],op[i].op.move.d[1],
		 op[i].op.move.d[2]);
	  if (op[i].op.move.p != NULL)
	    {
	      printf("\tknob: %s",op[i].op.move.p->name);
	    }
	  tmp = make_translate( op[i].op.move.d[0],
				op[i].op.move.d[1],
				op[i].op.move.d[2]);
	  matrix_mult(peek(systems), tmp);
	  copy_matrix(tmp, peek(systems));
	  tmp->lastcol = 0;
	  break;
	case SCALE:
	  printf("Scale: %6.2f %6.2f %6.2f",
		 op[i].op.scale.d[0],op[i].op.scale.d[1],
		 op[i].op.scale.d[2]);
	  if (op[i].op.scale.p != NULL)
	    {
	      printf("\tknob: %s",op[i].op.scale.p->name);
	    }
	  tmp = make_scale( op[i].op.scale.d[0],
			    op[i].op.scale.d[1],
			    op[i].op.scale.d[2]);
	  matrix_mult(peek(systems), tmp);
	  copy_matrix(tmp, peek(systems));
	  tmp->lastcol = 0;
	  break;
	case ROTATE:
	  printf("Rotate: axis: %6.2f degrees: %6.2f",
		 op[i].op.rotate.axis,
		 op[i].op.rotate.degrees);
	  if (op[i].op.rotate.p != NULL)
	    {
	      printf("\tknob: %s",op[i].op.rotate.p->name);
	    }
	  theta =  op[i].op.rotate.degrees * (M_PI / 180);
	  if (op[i].op.rotate.axis == 0 )
	    tmp = make_rotX( theta );
	  else if (op[i].op.rotate.axis == 1 )
	    tmp = make_rotY( theta );
	  else
	    tmp = make_rotZ( theta );
	  
	  matrix_mult(peek(systems), tmp);
	  copy_matrix(tmp, peek(systems));
	  tmp->lastcol = 0;
	  break;
	case PUSH:
	  printf("Push");
	  push(systems);
	  break;
	case POP:
	  printf("Pop");
	  pop(systems);
	  break;
	case SAVE:
	  printf("Save: %s",op[i].op.save.p->name);
	  save_extension(t, op[i].op.save.p->name);
	  break;
	case DISPLAY:
	  printf("Display");
	  display(t);
	  break;
	}
      printf("\n");
    }
  }
}
