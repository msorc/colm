/*
 *  Copyright 2007-2012 Adrian Thurston <thurston@complang.org>
 */

/*  This file is part of Colm.
 *
 *  Colm is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Colm is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Colm; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include <colm/pdarun.h>
#include <colm/tree.h>
#include <colm/pool.h>
#include <colm/bytecode.h>
#include <colm/debug.h>
#include <colm/map.h>
#include <colm/struct.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#define true 1
#define false 0

#define BUFFER_INITIAL_SIZE 4096

kid_t *allocAttrs( program_t *prg, long length )
{
	kid_t *cur = 0;
	long i;
	for ( i = 0; i < length; i++ ) {
		kid_t *next = cur;
		cur = kid_allocate( prg );
		cur->next = next;
	}
	return cur;
}

void freeAttrs( program_t *prg, kid_t *attrs )
{
	kid_t *cur = attrs;
	while ( cur != 0 ) {
		kid_t *next = cur->next;
		kid_free( prg, cur );
		cur = next;
	}
}

void freeKidList( program_t *prg, kid_t *kid )
{
	while ( kid != 0 ) {
		kid_t *next = kid->next;
		kid_free( prg, kid );
		kid = next;
	}
}

static void colm_tree_set_attr( tree_t *tree, long pos, tree_t *val )
{
	long i;
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	for ( i = 0; i < pos; i++ )
		kid = kid->next;
	kid->tree = val;
}

tree_t *colm_get_attr( tree_t *tree, long pos )
{
	long i;
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	for ( i = 0; i < pos; i++ )
		kid = kid->next;
	return kid->tree;
}


tree_t *colm_get_repeat_next( tree_t *tree )
{
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	return kid->next->tree;
}

tree_t *colm_get_repeat_val( tree_t *tree )
{
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;
	
	return kid->tree;
}

int colm_repeat_end( tree_t *tree )
{
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	return kid == 0;
}

int colm_list_last( tree_t *tree )
{
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	return kid->next == 0;
}

kid_t *getAttrKid( tree_t *tree, long pos )
{
	long i;
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	for ( i = 0; i < pos; i++ )
		kid = kid->next;
	return kid;
}

kid_t *kidListConcat( kid_t *list1, kid_t *list2 )
{
	if ( list1 == 0 )
		return list2;
	else if ( list2 == 0 )
		return list1;

	kid_t *dest = list1;
	while ( dest->next != 0 )
		dest = dest->next;
	dest->next = list2;
	return list1;
}

tree_t *colm_construct_pointer( program_t *prg, value_t value )
{
	pointer_t *pointer = (pointer_t*) tree_allocate( prg );
	pointer->id = LEL_ID_PTR;
	pointer->value = value;
	
	return (tree_t*)pointer;
}

value_t colm_get_pointer_val( tree_t *ptr )
{
	return ((pointer_t*)ptr)->value;
}


tree_t *colm_construct_term( program_t *prg, word_t id, head_t *tokdata )
{
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;

	tree_t *tree = tree_allocate( prg );
	tree->id = id;
	tree->refs = 0;
	tree->tokdata = tokdata;

	int objectLength = lelInfo[tree->id].objectLength;
	tree->child = allocAttrs( prg, objectLength );

	return tree;
}


kid_t *constructKid( program_t *prg, tree_t **bindings, kid_t *prev, long pat );

static kid_t *constructIgnoreList( program_t *prg, long ignoreInd )
{
	struct pat_cons_node *nodes = prg->rtd->patReplNodes;

	kid_t *first = 0, *last = 0;
	while ( ignoreInd >= 0 ) {
		head_t *ignoreData = colm_string_alloc_pointer( prg, nodes[ignoreInd].data,
				nodes[ignoreInd].length );

		tree_t *ignTree = tree_allocate( prg );
		ignTree->refs = 1;
		ignTree->id = nodes[ignoreInd].id;
		ignTree->tokdata = ignoreData;

		kid_t *ignKid = kid_allocate( prg );
		ignKid->tree = ignTree;
		ignKid->next = 0;

		if ( last == 0 )
			first = ignKid;
		else
			last->next = ignKid;

		ignoreInd = nodes[ignoreInd].next;
		last = ignKid;
	}

	return first;
}

static kid_t *constructLeftIgnoreList( program_t *prg, long pat )
{
	struct pat_cons_node *nodes = prg->rtd->patReplNodes;
	return constructIgnoreList( prg, nodes[pat].leftIgnore );
}

static kid_t *constructRightIgnoreList( program_t *prg, long pat )
{
	struct pat_cons_node *nodes = prg->rtd->patReplNodes;
	return constructIgnoreList( prg, nodes[pat].rightIgnore );
}

static void insLeftIgnore( program_t *prg, tree_t *tree, tree_t *ignoreList )
{
	assert( ! (tree->flags & AF_LEFT_IGNORE) );

	/* Allocate. */
	kid_t *kid = kid_allocate( prg );
	kid->tree = ignoreList;
	colm_tree_upref( ignoreList );

	/* Attach it. */
	kid->next = tree->child;
	tree->child = kid;

	tree->flags |= AF_LEFT_IGNORE;
}

static void insRightIgnore( program_t *prg, tree_t *tree, tree_t *ignoreList )
{
	assert( ! (tree->flags & AF_RIGHT_IGNORE) );

	/* Insert an ignore head in the child list. */
	kid_t *kid = kid_allocate( prg );
	kid->tree = ignoreList;
	colm_tree_upref( ignoreList );

	/* Attach it. */
	if ( tree->flags & AF_LEFT_IGNORE ) {
		kid->next = tree->child->next;
		tree->child->next = kid;
	}
	else {
		kid->next = tree->child;
		tree->child = kid;
	}

	tree->flags |= AF_RIGHT_IGNORE;
}

tree_t *push_right_ignore( program_t *prg, tree_t *pushTo, tree_t *rightIgnore )
{
	/* About to alter the data tree. Split first. */
	pushTo = splitTree( prg, pushTo );

	if ( pushTo->flags & AF_RIGHT_IGNORE ) {
		/* The previous token already has a right ignore. Merge by
		 * attaching it as a left ignore of the new list. */
		kid_t *curIgnore = treeRightIgnoreKid( prg, pushTo );
		insLeftIgnore( prg, rightIgnore, curIgnore->tree );

		/* Replace the current ignore. Safe to access refs here because we just
		 * upreffed it in insLeftIgnore. */
		curIgnore->tree->refs -= 1;
		curIgnore->tree = rightIgnore;
		colm_tree_upref( rightIgnore );
	}
	else {
		/* Attach The ignore list. */
		insRightIgnore( prg, pushTo, rightIgnore );
	}

	return pushTo;
}

tree_t *push_left_ignore( program_t *prg, tree_t *pushTo, tree_t *leftIgnore )
{
	pushTo = splitTree( prg, pushTo );

	/* Attach as left ignore to the token we are sending. */
	if ( pushTo->flags & AF_LEFT_IGNORE ) {
		/* The token already has a left-ignore. Merge by attaching it as a
		 * right ignore of the new list. */
		kid_t *curIgnore = treeLeftIgnoreKid( prg, pushTo );
		insRightIgnore( prg, leftIgnore, curIgnore->tree );

		/* Replace the current ignore. Safe to upref here because we just
		 * upreffed it in insRightIgnore. */
		curIgnore->tree->refs -= 1;
		curIgnore->tree = leftIgnore;
		colm_tree_upref( leftIgnore );
	}
	else {
		/* Attach the ignore list. */
		insLeftIgnore( prg, pushTo, leftIgnore );
	}

	return pushTo;
}

static void remLeftIgnore( program_t *prg, tree_t **sp, tree_t *tree )
{
	assert( tree->flags & AF_LEFT_IGNORE );

	kid_t *next = tree->child->next;
	colm_tree_downref( prg, sp, tree->child->tree );
	kid_free( prg, tree->child );
	tree->child = next;

	tree->flags &= ~AF_LEFT_IGNORE;
}

static void remRightIgnore( program_t *prg, tree_t **sp, tree_t *tree )
{
	assert( tree->flags & AF_RIGHT_IGNORE );

	if ( tree->flags & AF_LEFT_IGNORE ) {
		kid_t *next = tree->child->next->next;
		colm_tree_downref( prg, sp, tree->child->next->tree );
		kid_free( prg, tree->child->next );
		tree->child->next = next;
	}
	else {
		kid_t *next = tree->child->next;
		colm_tree_downref( prg, sp, tree->child->tree );
		kid_free( prg, tree->child );
		tree->child = next;
	}

	tree->flags &= ~AF_RIGHT_IGNORE;
}

tree_t *popRightIgnore( program_t *prg, tree_t **sp, tree_t *popFrom, tree_t **rightIgnore )
{
	/* Modifying the tree we are detaching from. */
	popFrom = splitTree( prg, popFrom );

	kid_t *riKid = treeRightIgnoreKid( prg, popFrom );

	/* If the right ignore has a left ignore, then that was the original
	 * right ignore. */
	kid_t *li = treeLeftIgnoreKid( prg, riKid->tree );
	if ( li != 0 ) {
		colm_tree_upref( li->tree );
		remLeftIgnore( prg, sp, riKid->tree );
		*rightIgnore = riKid->tree;
		colm_tree_upref( *rightIgnore );
		riKid->tree = li->tree;
	}
	else  {
		*rightIgnore = riKid->tree;
		colm_tree_upref( *rightIgnore );
		remRightIgnore( prg, sp, popFrom );
	}

	return popFrom;
}

tree_t *popLeftIgnore( program_t *prg, tree_t **sp, tree_t *popFrom, tree_t **leftIgnore )
{
	/* Modifying, make the write safe. */
	popFrom = splitTree( prg, popFrom );

	kid_t *liKid = treeLeftIgnoreKid( prg, popFrom );

	/* If the left ignore has a right ignore, then that was the original
	 * left ignore. */
	kid_t *ri = treeRightIgnoreKid( prg, liKid->tree );
	if ( ri != 0 ) {
		colm_tree_upref( ri->tree );
		remRightIgnore( prg, sp, liKid->tree );
		*leftIgnore = liKid->tree;
		colm_tree_upref( *leftIgnore );
		liKid->tree = ri->tree;
	}
	else {
		*leftIgnore = liKid->tree;
		colm_tree_upref( *leftIgnore );
		remLeftIgnore( prg, sp, popFrom );
	}

	return popFrom;
}

tree_t *colm_construct_object( program_t *prg, kid_t *kid, tree_t **bindings, long langElId )
{
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;
	tree_t *tree = 0;

	tree = tree_allocate( prg );
	tree->id = langElId;
	tree->refs = 1;
	tree->tokdata = 0;
	tree->prod_num = 0;

	int objectLength = lelInfo[tree->id].objectLength;

	kid_t *attrs = allocAttrs( prg, objectLength );
	kid_t *child = 0;

	tree->child = kidListConcat( attrs, child );

	return tree;
}

/* Returns an uprefed tree. Saves us having to downref and bindings to zero to
 * return a zero-ref tree. */
tree_t *colm_construct_tree( program_t *prg, kid_t *kid, tree_t **bindings, long pat )
{
	struct pat_cons_node *nodes = prg->rtd->patReplNodes;
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;
	tree_t *tree = 0;

	if ( nodes[pat].bindId > 0 ) {
		/* All bindings have been uprefed. */
		tree = bindings[nodes[pat].bindId];

		long ignore = nodes[pat].leftIgnore;
		tree_t *leftIgnore = 0;
		if ( ignore >= 0 ) {
			kid_t *ignore = constructLeftIgnoreList( prg, pat );

			leftIgnore = tree_allocate( prg );
			leftIgnore->id = LEL_ID_IGNORE;
			leftIgnore->child = ignore;

			tree = push_left_ignore( prg, tree, leftIgnore );
		}

		ignore = nodes[pat].rightIgnore;
		tree_t *rightIgnore = 0;
		if ( ignore >= 0 ) {
			kid_t *ignore = constructRightIgnoreList( prg, pat );

			rightIgnore = tree_allocate( prg );
			rightIgnore->id = LEL_ID_IGNORE;
			rightIgnore->child = ignore;

			tree = push_right_ignore( prg, tree, rightIgnore );
		}
	}
	else {
		tree = tree_allocate( prg );
		tree->id = nodes[pat].id;
		tree->refs = 1;
		tree->tokdata = nodes[pat].length == 0 ? 0 :
				colm_string_alloc_pointer( prg, 
				nodes[pat].data, nodes[pat].length );
		tree->prod_num = nodes[pat].prodNum;

		int objectLength = lelInfo[tree->id].objectLength;

		kid_t *attrs = allocAttrs( prg, objectLength );
		kid_t *child = constructKid( prg, bindings,
				0, nodes[pat].child );

		tree->child = kidListConcat( attrs, child );

		/* Right first, then left. */
		kid_t *ignore = constructRightIgnoreList( prg, pat );
		if ( ignore != 0 ) {
			tree_t *ignoreList = tree_allocate( prg );
			ignoreList->id = LEL_ID_IGNORE;
			ignoreList->refs = 1;
			ignoreList->child = ignore;

			kid_t *ignoreHead = kid_allocate( prg );
			ignoreHead->tree = ignoreList;
			ignoreHead->next = tree->child;
			tree->child = ignoreHead;

			tree->flags |= AF_RIGHT_IGNORE;
		}

		ignore = constructLeftIgnoreList( prg, pat );
		if ( ignore != 0 ) {
			tree_t *ignoreList = tree_allocate( prg );
			ignoreList->id = LEL_ID_IGNORE;
			ignoreList->refs = 1;
			ignoreList->child = ignore;

			kid_t *ignoreHead = kid_allocate( prg );
			ignoreHead->tree = ignoreList;
			ignoreHead->next = tree->child;
			tree->child = ignoreHead;

			tree->flags |= AF_LEFT_IGNORE;
		}

		int i;
		for ( i = 0; i < lelInfo[tree->id].numCaptureAttr; i++ ) {
			long ci = pat+1+i;
			CaptureAttr *ca = prg->rtd->captureAttr + lelInfo[tree->id].captureAttr + i;
			tree_t *attr = tree_allocate( prg );
			attr->id = nodes[ci].id;
			attr->refs = 1;
			attr->tokdata = nodes[ci].length == 0 ? 0 :
					colm_string_alloc_pointer( prg, 
					nodes[ci].data, nodes[ci].length );

			colm_tree_set_attr( tree, ca->offset, attr );
		}
	}

	return tree;
}

kid_t *constructKid( program_t *prg, tree_t **bindings, kid_t *prev, long pat )
{
	struct pat_cons_node *nodes = prg->rtd->patReplNodes;
	kid_t *kid = 0;

	if ( pat != -1 ) {
		kid = kid_allocate( prg );
		kid->tree = colm_construct_tree( prg, kid, bindings, pat );

		/* Recurse down next. */
		kid_t *next = constructKid( prg, bindings,
				kid, nodes[pat].next );

		kid->next = next;
	}

	return kid;
}

tree_t *colm_construct_token( program_t *prg, tree_t **args, long nargs )
{
	value_t idInt = (value_t)args[0];
	str_t *textStr = (str_t*)args[1];

	long id = (long)idInt;
	head_t *tokdata = stringCopy( prg, textStr->value );

	struct lang_el_info *lelInfo = prg->rtd->lelInfo;
	tree_t *tree;

	if ( lelInfo[id].ignore ) {
		tree = tree_allocate( prg );
		tree->refs = 1;
		tree->id = id;
		tree->tokdata = tokdata;
	}
	else {
		long objectLength = lelInfo[id].objectLength;
		assert( nargs-2 <= objectLength );

		kid_t *attrs = allocAttrs( prg, objectLength );

		tree = tree_allocate( prg );
		tree->id = id;
		tree->refs = 1;
		tree->tokdata = tokdata;

		tree->child = attrs;

		long i;
		for ( i = 2; i < nargs; i++ ) {
			colm_tree_set_attr( tree, i-2, args[i] );
			colm_tree_upref( colm_get_attr( tree, i-2 ) );
		}
	}
	return tree;
}

tree_t *castTree( program_t *prg, int langElId, tree_t *tree )
{
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;

	/* Need to keep a lookout for next down. If 
	 * copying it, return the copy. */
	tree_t *newTree = tree_allocate( prg );

	newTree->id = langElId;
	newTree->tokdata = stringCopy( prg, tree->tokdata );

	/* Invalidate the production number. */
	newTree->prod_num = -1;

	/* Copy the child list. Start with ignores, then the list. */
	kid_t *child = tree->child, *last = 0;

	/* Flags we are interested in. */
	newTree->flags |= tree->flags & ( AF_LEFT_IGNORE | AF_RIGHT_IGNORE );

	int ignores = 0;
	if ( tree->flags & AF_LEFT_IGNORE )
		ignores += 1;
	if ( tree->flags & AF_RIGHT_IGNORE )
		ignores += 1;

	/* Igores. */
	while ( ignores-- > 0 ) {
		kid_t *newKid = kid_allocate( prg );

		newKid->tree = child->tree;
		newKid->next = 0;
		newKid->tree->refs += 1;

		/* Store the first child. */
		if ( last == 0 )
			newTree->child = newKid;
		else
			last->next = newKid;

		child = child->next;
		last = newKid;
	}

	/* Skip over the source's attributes. */
	int objectLength = lelInfo[tree->id].objectLength;
	while ( objectLength-- > 0 )
		child = child->next;

	/* Allocate the target type's kids. */
	objectLength = lelInfo[langElId].objectLength;
	while ( objectLength-- > 0 ) {
		kid_t *newKid = kid_allocate( prg );

		newKid->tree = 0;
		newKid->next = 0;

		/* Store the first child. */
		if ( last == 0 )
			newTree->child = newKid;
		else
			last->next = newKid;

		last = newKid;
	}
	
	/* Copy the source's children. */
	while ( child != 0 ) {
		kid_t *newKid = kid_allocate( prg );

		newKid->tree = child->tree;
		newKid->next = 0;
		newKid->tree->refs += 1;

		/* Store the first child. */
		if ( last == 0 )
			newTree->child = newKid;
		else
			last->next = newKid;

		child = child->next;
		last = newKid;
	}
	
	return newTree;
}

tree_t *makeTree( program_t *prg, tree_t **args, long nargs )
{
	value_t idInt = (value_t)args[0];

	long id = (long)idInt;
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;

	tree_t *tree = tree_allocate( prg );
	tree->id = id;
	tree->refs = 1;

	long objectLength = lelInfo[id].objectLength;
	kid_t *attrs = allocAttrs( prg, objectLength );

	kid_t *last = 0, *child = 0;
	for ( id = 1; id < nargs; id++ ) {
		kid_t *kid = kid_allocate( prg );
		kid->tree = args[id];
		colm_tree_upref( kid->tree );

		if ( last == 0 )
			child = kid;
		else
			last->next = kid;

		last = kid;
	}

	tree->child = kidListConcat( attrs, child );

	return tree;
}

int testFalse( program_t *prg, tree_t *tree )
{
	int flse = ( 
		tree == 0 ||
		tree == prg->falseVal 
	);
	return flse;
}

kid_t *copyIgnoreList( program_t *prg, kid_t *ignoreHeader )
{
	kid_t *newHeader = kid_allocate( prg );
	kid_t *last = 0, *ic = (kid_t*)ignoreHeader->tree;
	while ( ic != 0 ) {
		kid_t *newIc = kid_allocate( prg );

		newIc->tree = ic->tree;
		newIc->tree->refs += 1;

		/* List pointers. */
		if ( last == 0 )
			newHeader->tree = (tree_t*)newIc;
		else
			last->next = newIc;

		ic = ic->next;
		last = newIc;
	}
	return newHeader;
}

kid_t *copyKidList( program_t *prg, kid_t *kidList )
{
	kid_t *newList = 0, *last = 0, *ic = kidList;

	while ( ic != 0 ) {
		kid_t *newIc = kid_allocate( prg );

		newIc->tree = ic->tree;
		colm_tree_upref( newIc->tree );

		/* List pointers. */
		if ( last == 0 )
			newList = newIc;
		else
			last->next = newIc;

		ic = ic->next;
		last = newIc;
	}
	return newList;
}

/* New tree has zero ref. */
tree_t *copyRealTree( program_t *prg, tree_t *tree, kid_t *oldNextDown, kid_t **newNextDown )
{
	/* Need to keep a lookout for next down. If 
	 * copying it, return the copy. */
	tree_t *newTree = tree_allocate( prg );

	newTree->id = tree->id;
	newTree->tokdata = stringCopy( prg, tree->tokdata );
	newTree->prod_num = tree->prod_num;

	/* Copy the child list. Start with ignores, then the list. */
	kid_t *child = tree->child, *last = 0;

	/* Left ignores. */
	if ( tree->flags & AF_LEFT_IGNORE ) {
		newTree->flags |= AF_LEFT_IGNORE;
//		kid_t *newHeader = copyIgnoreList( prg, child );
//
//		/* Always the head. */
//		newTree->child = newHeader;
//
//		child = child->next;
//		last = newHeader;
	}

	/* Right ignores. */
	if ( tree->flags & AF_RIGHT_IGNORE ) {
		newTree->flags |= AF_RIGHT_IGNORE;
//		kid_t *newHeader = copyIgnoreList( prg, child );
//		if ( last == 0 )
//			newTree->child = newHeader;
//		else
//			last->next = newHeader;
//		child = child->next;
//		last = newHeader;
	}

	/* Attributes and children. */
	while ( child != 0 ) {
		kid_t *newKid = kid_allocate( prg );

		/* Watch out for next down. */
		if ( child == oldNextDown )
			*newNextDown = newKid;

		newKid->tree = child->tree;
		newKid->next = 0;

		/* May be an attribute. */
		if ( newKid->tree != 0 )
			newKid->tree->refs += 1;

		/* Store the first child. */
		if ( last == 0 )
			newTree->child = newKid;
		else
			last->next = newKid;

		child = child->next;
		last = newKid;
	}
	
	return newTree;
}


tree_t *colm_copy_tree( program_t *prg, tree_t *tree, kid_t *oldNextDown, kid_t **newNextDown )
{
//	struct lang_el_info *lelInfo = prg->rtd->lelInfo;
//	long genericId = lelInfo[tree->id].genericId;
//	if ( genericId > 0 )
//		assert(false);
	if ( tree->id == LEL_ID_PTR )
		assert(false);
//	else if ( tree->id == LEL_ID_BOOL )
//		assert(false);
//	else if ( tree->id == LEL_ID_INT )
//		assert(false);
	else if ( tree->id == LEL_ID_STR )
		assert(false);
//	else if ( tree->id == LEL_ID_STREAM )
//		assert(false);
	else {
		tree = copyRealTree( prg, tree, oldNextDown, newNextDown );
	}

	assert( tree->refs == 0 );
	return tree;
}

tree_t *splitTree( program_t *prg, tree_t *tree )
{
	if ( tree != 0 ) {
		assert( tree->refs >= 1 );

		if ( tree->refs > 1 ) {
			kid_t *oldNextDown = 0, *newNextDown = 0;
			tree_t *newTree = colm_copy_tree( prg, tree, oldNextDown, &newNextDown );
			colm_tree_upref( newTree );

			/* Downref the original. Don't need to consider freeing because
			 * refs were > 1. */
			tree->refs -= 1;

			tree = newTree;
		}

		assert( tree->refs == 1 );
	}
	return tree;
}

/* We can't make recursive calls here since the tree we are freeing may be
 * very large. Need the VM stack. */
void treeFreeRec( program_t *prg, tree_t **sp, tree_t *tree )
{
	tree_t **top = vm_ptop();
//	struct lang_el_info *lelInfo;
//	long genericId;

free_tree:
//	lelInfo = prg->rtd->lelInfo;
//	genericId = lelInfo[tree->id].genericId;
//	assert( genericId == 0 );

	switch ( tree->id ) {
//	case LEL_ID_BOOL:
//	case LEL_ID_INT:
	case LEL_ID_PTR:
		tree_free( prg, tree );
		break;
	case LEL_ID_STR: {
		str_t *str = (str_t*) tree;
		stringFree( prg, str->value );
		tree_free( prg, tree );
		break;
	}
	default: { 
		if ( tree->id != LEL_ID_IGNORE )
			stringFree( prg, tree->tokdata );

		/* Attributes and grammar-based children. */
		kid_t *child = tree->child;
		while ( child != 0 ) {
			kid_t *next = child->next;
			vm_push_tree( child->tree );
			kid_free( prg, child );
			child = next;
		}

		tree_free( prg, tree );
		break;
	}}

	/* Any trees to downref? */
	while ( sp != top ) {
		tree = vm_pop_tree();
		if ( tree != 0 ) {
			assert( tree->refs > 0 );
			tree->refs -= 1;
			if ( tree->refs == 0 )
				goto free_tree;
		}
	}
}

void colm_tree_upref( tree_t *tree )
{
	if ( tree != 0 )
		tree->refs += 1;
}

void colm_tree_downref( program_t *prg, tree_t **sp, tree_t *tree )
{
	if ( tree != 0 ) {
		assert( tree->refs > 0 );
		tree->refs -= 1;
		if ( tree->refs == 0 )
			treeFreeRec( prg, sp, tree );
	}
}

/* We can't make recursive calls here since the tree we are freeing may be
 * very large. Need the VM stack. */
void objectFreeRec( program_t *prg, tree_t **sp, tree_t *tree )
{
	tree_t **top = vm_ptop();
//	struct lang_el_info *lelInfo;
//	long genericId;

free_tree:
//	lelInfo = prg->rtd->lelInfo;

	switch ( tree->id ) {
	case LEL_ID_STR: {
		str_t *str = (str_t*) tree;
		stringFree( prg, str->value );
		tree_free( prg, tree );
		break;
	}
//	case LEL_ID_BOOL:
//	case LEL_ID_INT: {
//		tree_free( prg, tree );
//		break;
//	}
	case LEL_ID_PTR: {
		tree_free( prg, tree );
		break;
	}
//	case LEL_ID_STREAM: {
//	}
	default: { 
		if ( tree->id != LEL_ID_IGNORE )
			stringFree( prg, tree->tokdata );

		/* Attributes and grammar-based children. */
		kid_t *child = tree->child;
		while ( child != 0 ) {
			kid_t *next = child->next;
			vm_push_tree( child->tree );
			kid_free( prg, child );
			child = next;
		}

		tree_free( prg, tree );
		break;
	}}

	/* Any trees to downref? */
	while ( sp != top ) {
		tree = vm_pop_tree();
		if ( tree != 0 ) {
			assert( tree->refs > 0 );
			tree->refs -= 1;
			if ( tree->refs == 0 )
				goto free_tree;
		}
	}
}

void objectDownref( program_t *prg, tree_t **sp, tree_t *tree )
{
	if ( tree != 0 ) {
		assert( tree->refs > 0 );
		tree->refs -= 1;
		if ( tree->refs == 0 )
			objectFreeRec( prg, sp, tree );
	}
}

/* Find the first child of a tree. */
kid_t *treeChild( program_t *prg, const tree_t *tree )
{
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	/* Skip over attributes. */
	long objectLength = lelInfo[tree->id].objectLength;
	long a;
	for ( a = 0; a < objectLength; a++ )
		kid = kid->next;

	return kid;
}

/* Detach at the first real child of a tree. */
kid_t *treeExtractChild( program_t *prg, tree_t *tree )
{
	struct lang_el_info *lelInfo = prg->rtd->lelInfo;
	kid_t *kid = tree->child, *last = 0;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	/* Skip over attributes. */
	long a, objectLength = lelInfo[tree->id].objectLength;
	for ( a = 0; a < objectLength; a++ ) {
		last = kid;
		kid = kid->next;
	}

	if ( last == 0 )
		tree->child = 0;
	else
		last->next = 0;

	return kid;
}


/* Find the first child of a tree. */
kid_t *treeAttr( program_t *prg, const tree_t *tree )
{
	kid_t *kid = tree->child;

	if ( tree->flags & AF_LEFT_IGNORE )
		kid = kid->next;
	if ( tree->flags & AF_RIGHT_IGNORE )
		kid = kid->next;

	return kid;
}

tree_t *treeLeftIgnore( program_t *prg, tree_t *tree )
{
	if ( tree->flags & AF_LEFT_IGNORE )
		return tree->child->tree;
	return 0;
}

tree_t *treeRightIgnore( program_t *prg, tree_t *tree )
{
	if ( tree->flags & AF_RIGHT_IGNORE ) {
		if ( tree->flags & AF_LEFT_IGNORE )
			return tree->child->next->tree;
		else
			return tree->child->tree;
	}
	return 0;
}

kid_t *treeLeftIgnoreKid( program_t *prg, tree_t *tree )
{
	if ( tree->flags & AF_LEFT_IGNORE )
		return tree->child;
	return 0;
}

kid_t *treeRightIgnoreKid( program_t *prg, tree_t *tree )
{
	if ( tree->flags & AF_RIGHT_IGNORE ) {
		if ( tree->flags & AF_LEFT_IGNORE )
			return tree->child->next;
		else
			return tree->child;
	}
	return 0;
}

void refSetValue( program_t *prg, tree_t **sp, ref_t *ref, tree_t *v )
{
	colm_tree_downref( prg, sp, ref->kid->tree );
	ref->kid->tree = v;
}

tree_t *getRhsEl( program_t *prg, tree_t *lhs, long position )
{
	kid_t *pos = treeChild( prg, lhs );
	while ( position > 0 ) {
		pos = pos->next;
		position -= 1;
	}
	return pos->tree;
}

kid_t *getRhsElKid( program_t *prg, tree_t *lhs, long position )
{
	kid_t *pos = treeChild( prg, lhs );
	while ( position > 0 ) {
		pos = pos->next;
		position -= 1;
	}
	return pos;
}


tree_t *colm_get_rhs_val( program_t *prg, tree_t *tree, int *a )
{
	int i, len = a[0];
	for ( i = 0; i < len; i++ ) {
		int prod_num = a[1 + i * 2];
		int childNum = a[1 + i * 2 + 1];
		if ( tree->prod_num == prod_num )
			return getRhsEl( prg, tree, childNum );
	}
	return 0;
}

void colm_tree_set_field( program_t *prg, tree_t *tree, long field, tree_t *value )
{
	assert( tree->refs == 1 );
	if ( value != 0 )
		assert( value->refs >= 1 );
	colm_tree_set_attr( tree, field, value );
}

tree_t *colm_tree_get_field( tree_t *tree, word_t field )
{
	return colm_get_attr( tree, field );
}

kid_t *getFieldKid( tree_t *tree, word_t field )
{
	return getAttrKid( tree, field );
}

tree_t *getFieldSplit( program_t *prg, tree_t *tree, word_t field )
{
	tree_t *val = colm_get_attr( tree, field );
	tree_t *split = splitTree( prg, val );
	colm_tree_set_attr( tree, field, split );
	return split;
}

/* This must traverse in the same order that the bindId assignments are done
 * in. */
int matchPattern( tree_t **bindings, program_t *prg, long pat, kid_t *kid, int checkNext )
{
	struct pat_cons_node *nodes = prg->rtd->patReplNodes;

	/* match node, recurse on children. */
	if ( pat != -1 && kid != 0 ) {
		if ( nodes[pat].id == kid->tree->id ) {
			/* If the pattern node has data, then this means we need to match
			 * the data against the token data. */
			if ( nodes[pat].data != 0 ) {
				/* Check the length of token text. */
				if ( nodes[pat].length != stringLength( kid->tree->tokdata ) )
					return false;

				/* Check the token text data. */
				if ( nodes[pat].length > 0 && memcmp( nodes[pat].data, 
						stringData( kid->tree->tokdata ), nodes[pat].length ) != 0 )
					return false;
			}

			/* No failure, all okay. */
			if ( nodes[pat].bindId > 0 ) {
				bindings[nodes[pat].bindId] = kid->tree;
			}

			/* If we didn't match a terminal duplicate of a nonterm then check
			 * down the children. */
			if ( !nodes[pat].stop ) {
				/* Check for failure down child branch. */
				int childCheck = matchPattern( bindings, prg, 
						nodes[pat].child, treeChild( prg, kid->tree ), true );
				if ( ! childCheck )
					return false;
			}

			/* If checking next, then look for failure there. */
			if ( checkNext ) {
				int nextCheck = matchPattern( bindings, prg, 
						nodes[pat].next, kid->next, true );
				if ( ! nextCheck )
					return false;
			}

			return true;
		}
	}
	else if ( pat == -1 && kid == 0 ) {
		/* Both null is a match. */
		return 1;
	}

	return false;
}


long colm_cmp_tree( program_t *prg, const tree_t *tree1, const tree_t *tree2 )
{
	long cmpres = 0;
	if ( tree1 == 0 ) {
		if ( tree2 == 0 )
			return 0;
		else
			return -1;
	}
	else if ( tree2 == 0 )
		return 1;
	else if ( tree1->id < tree2->id )
		return -1;
	else if ( tree1->id > tree2->id )
		return 1;
	else if ( tree1->id == LEL_ID_PTR ) {
		if ( ((pointer_t*)tree1)->value < ((pointer_t*)tree2)->value )
			return -1;
		else if ( ((pointer_t*)tree1)->value > ((pointer_t*)tree2)->value )
			return 1;
	}
	else if ( tree1->id == LEL_ID_STR ) {
		cmpres = cmpString( ((str_t*)tree1)->value, ((str_t*)tree2)->value );
		if ( cmpres != 0 )
			return cmpres;
	}
	else {
		if ( tree1->tokdata == 0 && tree2->tokdata != 0 )
			return -1;
		else if ( tree1->tokdata != 0 && tree2->tokdata == 0 )
			return 1;
		else if ( tree1->tokdata != 0 && tree2->tokdata != 0 ) {
			cmpres = cmpString( tree1->tokdata, tree2->tokdata );
			if ( cmpres != 0 )
				return cmpres;
		}
	}

	kid_t *kid1 = treeChild( prg, tree1 );
	kid_t *kid2 = treeChild( prg, tree2 );

	while ( true ) {
		if ( kid1 == 0 && kid2 == 0 )
			return 0;
		else if ( kid1 == 0 && kid2 != 0 )
			return -1;
		else if ( kid1 != 0 && kid2 == 0 )
			return 1;
		else {
			cmpres = colm_cmp_tree( prg, kid1->tree, kid2->tree );
			if ( cmpres != 0 )
				return cmpres;
		}
		kid1 = kid1->next;
		kid2 = kid2->next;
	}
}


void splitRef( program_t *prg, tree_t ***psp, ref_t *fromRef )
{
	/* Go up the chain of kids, turing the pointers down. */
	ref_t *last = 0, *ref = fromRef, *next = 0;
	while ( ref->next != 0 ) {
		next = ref->next;
		ref->next = last;
		last = ref;
		ref = next;
	}
	ref->next = last;

	/* Now traverse the list, which goes down. */
	while ( ref != 0 ) {
		if ( ref->kid->tree->refs > 1 ) {
			ref_t *nextDown = ref->next;
			while ( nextDown != 0 && nextDown->kid == ref->kid )
				nextDown = nextDown->next;

			kid_t *oldNextKidDown = nextDown != 0 ? nextDown->kid : 0;
			kid_t *newNextKidDown = 0;

			tree_t *newTree = colm_copy_tree( prg, ref->kid->tree, 
					oldNextKidDown, &newNextKidDown );
			colm_tree_upref( newTree );
			
			/* Downref the original. Don't need to consider freeing because
			 * refs were > 1. */
			ref->kid->tree->refs -= 1;

			while ( ref != 0 && ref != nextDown ) {
				next = ref->next;
				ref->next = 0;

				ref->kid->tree = newTree;
				ref = next;
			}

			/* Correct kid pointers down from ref. */
			while ( nextDown != 0 && nextDown->kid == oldNextKidDown ) {
				nextDown->kid = newNextKidDown;
				nextDown = nextDown->next;
			}
		}
		else {
			/* Reset the list as we go down. */
			next = ref->next;
			ref->next = 0;
			ref = next;
		}
	}
}

tree_t *setListMem( list_t *list, half_t field, tree_t *value )
{
	if ( value != 0 )
		assert( value->refs >= 1 );

	tree_t *existing = 0;
	switch ( field ) {
		case 0:
//			existing = list->head->value;
//			list->head->value = value;
			break;
		case 1:
//			existing = list->tail->value;
//			list->tail->value = value;
			break;
		default:
			assert( false );
			break;
	}
	return existing;
}

struct tree_pair mapRemove( program_t *prg, map_t *map, tree_t *key )
{
	map_el_t *mapEl = mapImplFind( prg, map, key );
	struct tree_pair result = { 0, 0 };
	if ( mapEl != 0 ) {
		mapDetach( prg, map, mapEl );
		result.key = mapEl->key;
		//mapElFree( prg, mapEl );
	}

	return result;
}

#if 0
tree_t *mapUnstore( program_t *prg, map_t *map, tree_t *key, tree_t *existing )
{
	tree_t *stored = 0;
	if ( existing == 0 ) {
		map_el_t *mapEl = mapDetachByKey( prg, map, key );
		// stored = mapEl->tree;
		mapElFree( prg, mapEl );
	}
	else {
		map_el_t *mapEl = mapImplFind( prg, map, key );
		// stored = mapEl->tree;
		//mapEl->tree = existing;
	}
	return stored;
}
#endif

tree_t *mapFind( program_t *prg, map_t *map, tree_t *key )
{
//	map_el_t *mapEl = mapImplFind( prg, map, key );
//	return mapEl == 0 ? 0 : mapEl->tree;
	return 0;
}

long mapLength( map_t *map )
{
	return map->treeSize;
}

void listPushTail( program_t *prg, list_t *list, tree_t *val )
{
//	if ( val != 0 )
//		assert( val->refs >= 1 );
//	list_el_t *listEl = colm_list_el_new( prg );
//	listEl->value = val;
//	listAppend( list, listEl );
}

void listPushHead( program_t *prg, list_t *list, tree_t *val )
{
//	if ( val != 0 )
//		assert( val->refs >= 1 );
//	list_el_t *listEl = listElAllocate( prg );
//	listEl->value = val;
//	listPrepend( list, listEl );
}

tree_t *listRemoveEnd( program_t *prg, list_t *list )
{
//	tree_t *tree = list->tail->value;
//	listElFree( prg, listDetachLast( list ) );
//	return tree;
	return 0;
}

tree_t *listRemoveHead( program_t *prg, list_t *list )
{
//	tree_t *tree = list->head;
//	listDetachFirst( list );
//	return tree;
	return 0;
}

tree_t *getParserMem( parser_t *parser, word_t field )
{
	tree_t *result = 0;
	switch ( field ) {
		case 0: {
			result = parser->result;
			break;
		}
		case 1: {
			struct pda_run *pdaRun = parser->pdaRun;
			result = pdaRun->parseErrorText;
			break;
		}
		default: {
			assert( false );
			break;
		}
	}
	return result;
}

tree_t *getListMemSplit( program_t *prg, list_t *list, word_t field )
{
	tree_t *sv = 0;
	switch ( field ) {
		case 0: 
//			sv = splitTree( prg, list->head->value );
//			list->head->value = sv; 
			break;
		case 1: 
//			sv = splitTree( prg, list->tail->value );
//			list->tail->value = sv; 
			break;
		default:
			assert( false );
			break;
	}
	return sv;
}


#if 0
int mapInsert( program_t *prg, map_t *map, tree_t *key, tree_t *element )
{
	map_el_t *mapEl = mapInsertKey( prg, map, key, 0 );

	if ( mapEl != 0 ) {
		//mapEl->tree = element;
		return true;
	}

	return false;
}
#endif

#if 0
void mapUnremove( program_t *prg, map_t *map, tree_t *key, tree_t *element )
{
	map_el_t *mapEl = mapInsertKey( prg, map, key, 0 );
	assert( mapEl != 0 );
	//mapEl->tree = element;
}
#endif

#if 0
tree_t *mapUninsert( program_t *prg, map_t *map, tree_t *key )
{
	map_el_t *el = mapDetachByKey( prg, map, key );
//	tree_t *val = el->tree;
	mapElFree( prg, el );
//	return val;
	return 0;
}
#endif

#if 0
tree_t *mapStore( program_t *prg, map_t *map, tree_t *key, tree_t *element )
{
	tree_t *oldTree = 0;
	map_el_t *elInTree = 0;
	map_el_t *mapEl = mapInsertKey( prg, map, key, &elInTree );

//	if ( mapEl != 0 )
//		mapEl->tree = element;
//	else {
//		/* Element with key exists. Overwriting the value. */
//		oldTree = elInTree->tree;
//		elInTree->tree = element;
//	}

	return oldTree;
}
#endif

static tree_t *treeSearchKid( program_t *prg, kid_t *kid, long id )
{
	/* This node the one? */
	if ( kid->tree->id == id )
		return kid->tree;

	tree_t *res = 0;

	/* Search children. */
	kid_t *child = treeChild( prg, kid->tree );
	if ( child != 0 )
		res = treeSearchKid( prg, child, id );
	
	/* Search siblings. */
	if ( res == 0 && kid->next != 0 )
		res = treeSearchKid( prg, kid->next, id );

	return res;	
}

tree_t *treeSearch( program_t *prg, tree_t *tree, long id )
{
	tree_t *res = 0;
	if ( tree->id == id )
		res = tree;
	else {
		kid_t *child = treeChild( prg, tree );
		if ( child != 0 )
			res = treeSearchKid( prg, child, id );
	}
	return res;
}

static location_t *locSearchKid( program_t *prg, kid_t *kid )
{
	/* This node the one? */
	if ( kid->tree->tokdata != 0 && kid->tree->tokdata->location != 0 )
		return kid->tree->tokdata->location;

	location_t *res = 0;

	/* Search children. */
	kid_t *child = treeChild( prg, kid->tree );
	if ( child != 0 )
		res = locSearchKid( prg, child );
	
	/* Search siblings. */
	if ( res == 0 && kid->next != 0 )
		res = locSearchKid( prg, kid->next );

	return res;	
}

location_t *locSearch( program_t *prg, tree_t *tree )
{
	location_t *res = 0;
	if ( tree->tokdata != 0 && tree->tokdata->location != 0 )
		return tree->tokdata->location;

	kid_t *child = treeChild( prg, tree );
	if ( child != 0 )
		res = locSearchKid( prg, child );

	return res;
}

struct colm_location *colm_find_location( program_t *prg, tree_t *tree )
{
	return locSearch( prg, tree );
}

/*
 * tree_t Printing
 */

void xmlEscapeData( struct colm_print_args *printArgs, const char *data, long len )
{
	int i;
	for ( i = 0; i < len; i++ ) {
		if ( data[i] == '<' )
			printArgs->out( printArgs, "&lt;", 4 );
		else if ( data[i] == '>' )
			printArgs->out( printArgs, "&gt;", 4 );
		else if ( data[i] == '&' )
			printArgs->out( printArgs, "&amp;", 5 );
		else if ( (32 <= data[i] && data[i] <= 126) || 
				data[i] == '\t' || data[i] == '\n' || data[i] == '\r' )
		{
			printArgs->out( printArgs, &data[i], 1 );
		}
		else {
			char out[64];
			sprintf( out, "&#%u;", ((unsigned)data[i]) );
			printArgs->out( printArgs, out, strlen(out) );
		}
	}
}

void initStrCollect( StrCollect *collect )
{
	collect->data = (char*) malloc( BUFFER_INITIAL_SIZE );
	collect->allocated = BUFFER_INITIAL_SIZE;
	collect->length = 0;
}

void strCollectDestroy( StrCollect *collect )
{
	free( collect->data );
}

void strCollectAppend( StrCollect *collect, const char *data, long len )
{
	long newLen = collect->length + len;
	if ( newLen > collect->allocated ) {
		collect->allocated = newLen * 2;
		collect->data = (char*) realloc( collect->data, collect->allocated );
	}
	memcpy( collect->data + collect->length, data, len );
	collect->length += len;
}
		
void strCollectClear( StrCollect *collect )
{
	collect->length = 0;
}

#define INT_SZ 32

void printStr( struct colm_print_args *printArgs, head_t *str )
{
	printArgs->out( printArgs, (char*)(str->data), str->length );
}

void appendCollect( struct colm_print_args *args, const char *data, int length )
{
	strCollectAppend( (StrCollect*) args->arg, data, length );
}

void appendFile( struct colm_print_args *args, const char *data, int length )
{
	fwrite( data, 1, length, (FILE*)args->arg );
}

void appendFd( struct colm_print_args *args, const char *data, int length )
{
	int fd = (long)args->arg;
	int res = write( fd, data, length );
	if ( res < 0 )
		message( "write error on fd: %d: %s\n", fd, strerror(errno) );
}

tree_t *treeTrim( struct colm_program *prg, tree_t **sp, tree_t *tree )
{
	debug( prg, REALM_PARSE, "attaching left ignore\n" );

	/* Make the ignore list for the left-ignore. */
	tree_t *leftIgnore = tree_allocate( prg );
	leftIgnore->id = LEL_ID_IGNORE;
	leftIgnore->flags |= AF_SUPPRESS_RIGHT;

	tree = push_left_ignore( prg, tree, leftIgnore );

	debug( prg, REALM_PARSE, "attaching ignore right\n" );

	/* Copy the ignore list first if we need to attach it as a right
	 * ignore. */
	tree_t *rightIgnore = 0;
	rightIgnore = tree_allocate( prg );
	rightIgnore->id = LEL_ID_IGNORE;
	rightIgnore->flags |= AF_SUPPRESS_LEFT;

	tree = push_right_ignore( prg, tree, rightIgnore );

	return tree;
}

enum ReturnType
{
	Done = 1,
	CollectIgnoreLeft,
	CollectIgnoreRight,
	RecIgnoreList,
	ChildPrint
};

enum VisitType
{
	IgnoreWrapper,
	IgnoreData,
	Term,
	NonTerm,
};

#define TF_TERM_SEEN 0x1

void printKid( program_t *prg, tree_t **sp, struct colm_print_args *printArgs, kid_t *kid )
{
	enum ReturnType rt;
	kid_t *parent = 0;
	kid_t *leadingIgnore = 0;
	enum VisitType visitType;
	int flags = 0;

	/* Iterate the kids passed in. We are expecting a next, which will allow us
	 * to print the trailing ignore list. */
	while ( kid != 0 ) {
		vm_push_type( enum ReturnType, Done );
		goto rec_call;
		rec_return_top:
		kid = kid->next;
	}

	return;

rec_call:
	if ( kid->tree == 0 )
		goto skip_null;

	/* If not currently skipping ignore data, then print it. Ignore data can
	 * be associated with terminals and nonterminals. */
	if ( kid->tree->flags & AF_LEFT_IGNORE ) {
		vm_push_kid( parent );
		vm_push_kid( kid );
		parent = kid;
		kid = treeLeftIgnoreKid( prg, kid->tree );
		vm_push_type( enum ReturnType, CollectIgnoreLeft );
		goto rec_call;
		rec_return_ign_left:
		kid = vm_pop_kid();
		parent = vm_pop_kid();
	}

	if ( kid->tree->id == LEL_ID_IGNORE )
		visitType = IgnoreWrapper;
	else if ( parent != 0 && parent->tree->id == LEL_ID_IGNORE )
		visitType = IgnoreData;
	else if ( kid->tree->id < prg->rtd->firstNonTermId )
		visitType = Term;
	else
		visitType = NonTerm;
	
	debug( prg, REALM_PRINT, "visit type: %d\n", visitType );

	if ( visitType == IgnoreData ) {
		debug( prg, REALM_PRINT, "putting %p on ignore list\n", kid->tree );
		kid_t *newIgnore = kid_allocate( prg );
		newIgnore->next = leadingIgnore;
		leadingIgnore = newIgnore;
		leadingIgnore->tree = kid->tree;
		goto skip_node;
	}

	if ( visitType == IgnoreWrapper ) {
		kid_t *newIgnore = kid_allocate( prg );
		newIgnore->next = leadingIgnore;
		leadingIgnore = newIgnore;
		leadingIgnore->tree = kid->tree;
		/* Don't skip. */
	}

	/* print leading ignore? Triggered by terminals. */
	if ( visitType == Term ) {
		/* Reverse the leading ignore list. */
		if ( leadingIgnore != 0 ) {
			kid_t *ignore = 0, *last = 0;

			/* Reverse the list and take the opportunity to implement the
			 * suppress left. */
			while ( true ) {
				kid_t *next = leadingIgnore->next;
				leadingIgnore->next = last;

				if ( leadingIgnore->tree->flags & AF_SUPPRESS_LEFT ) {
					/* We are moving left. Chop off the tail. */
					debug( prg, REALM_PRINT, "suppressing left\n" );
					freeKidList( prg, next );
					break;
				}

				if ( next == 0 )
					break;

				last = leadingIgnore;
				leadingIgnore = next;
			}

			/* Print the leading ignore list. Also implement the suppress right
			 * in the process. */
			if ( printArgs->comm && (!printArgs->trim || (flags & TF_TERM_SEEN && kid->tree->id > 0)) ) {	
				ignore = leadingIgnore;
				while ( ignore != 0 ) {
					if ( ignore->tree->flags & AF_SUPPRESS_RIGHT )
						break;

					if ( ignore->tree->id != LEL_ID_IGNORE ) {
						vm_push_type( enum VisitType, visitType );
						vm_push_kid( leadingIgnore );
						vm_push_kid( ignore );
						vm_push_kid( parent );
						vm_push_kid( kid );

						leadingIgnore = 0;
						kid = ignore;
						parent = 0;

						debug( prg, REALM_PRINT, "rec call on %p\n", kid->tree );
						vm_push_type( enum ReturnType, RecIgnoreList );
						goto rec_call;
						rec_return_il:

						kid = vm_pop_kid();
						parent = vm_pop_kid();
						ignore = vm_pop_kid();
						leadingIgnore = vm_pop_kid();
						visitType = vm_pop_type(enum VisitType);
					}

					ignore = ignore->next;
				}
			}

			/* Free the leading ignore list. */
			freeKidList( prg, leadingIgnore );
			leadingIgnore = 0;
		}
	}

	if ( visitType == Term || visitType == NonTerm ) {
		/* Open the tree. */
		printArgs->open_tree( prg, sp, printArgs, parent, kid );
	}

	if ( visitType == Term )
		flags |= TF_TERM_SEEN;

	if ( visitType == Term || visitType == IgnoreData ) {
		/* Print contents. */
		if ( kid->tree->id < prg->rtd->firstNonTermId ) {
			debug( prg, REALM_PRINT, "printing terminal %p\n", kid->tree );
			if ( kid->tree->id != 0 )
				printArgs->print_term( prg, sp, printArgs, kid );
		}
	}

	/* Print children. */
	kid_t *child = printArgs->attr ? 
		treeAttr( prg, kid->tree ) : 
		treeChild( prg, kid->tree );

	if ( child != 0 ) {
		vm_push_type( enum VisitType, visitType );
		vm_push_kid( parent );
		vm_push_kid( kid );
		parent = kid;
		kid = child;
		while ( kid != 0 ) {
			vm_push_type( enum ReturnType, ChildPrint );
			goto rec_call;
			rec_return:
			kid = kid->next;
		}
		kid = vm_pop_kid();
		parent = vm_pop_kid();
		visitType = vm_pop_type(enum VisitType);
	}

	if ( visitType == Term || visitType == NonTerm ) {
		/* close the tree. */
		printArgs->close_tree( prg, sp, printArgs, parent, kid );
	}

skip_node:

	/* If not currently skipping ignore data, then print it. Ignore data can
	 * be associated with terminals and nonterminals. */
	if ( kid->tree->flags & AF_RIGHT_IGNORE ) {
		debug( prg, REALM_PRINT, "right ignore\n" );
		vm_push_kid( parent );
		vm_push_kid( kid );
		parent = kid;
		kid = treeRightIgnoreKid( prg, kid->tree );
		vm_push_type( enum ReturnType, CollectIgnoreRight );
		goto rec_call;
		rec_return_ign_right:
		kid = vm_pop_kid();
		parent = vm_pop_kid();
	}

/* For skiping over content on null. */
skip_null:

	rt = vm_pop_type(enum ReturnType);
	switch ( rt ) {
		case Done:
			debug( prg, REALM_PRINT, "return: done\n" );
			goto rec_return_top;
			break;
		case CollectIgnoreLeft:
			debug( prg, REALM_PRINT, "return: ignore left\n" );
			goto rec_return_ign_left;
		case CollectIgnoreRight:
			debug( prg, REALM_PRINT, "return: ignore right\n" );
			goto rec_return_ign_right;
		case RecIgnoreList:
			debug( prg, REALM_PRINT, "return: ignore list\n" );
			goto rec_return_il;
		case ChildPrint:
			debug( prg, REALM_PRINT, "return: child print\n" );
			goto rec_return;
	}
}

void colm_print_tree_args( program_t *prg, tree_t **sp, struct colm_print_args *printArgs, tree_t *tree )
{
	if ( tree == 0 )
		printArgs->out( printArgs, "NIL", 3 );
	else {
		/* This term tree allows us to print trailing ignores. */
		tree_t termTree;
		memset( &termTree, 0, sizeof(termTree) );

		kid_t kid, term;
		term.tree = &termTree;
		term.next = 0;
		term.flags = 0;

		kid.tree = tree;
		kid.next = &term;
		kid.flags = 0;

		printKid( prg, sp, printArgs, &kid );
	}
}

void colm_print_term_tree( program_t *prg, tree_t **sp, struct colm_print_args *printArgs, kid_t *kid )
{
	debug( prg, REALM_PRINT, "printing term %p\n", kid->tree );

	if ( kid->tree->id == LEL_ID_PTR ) {
		char buf[INT_SZ];
		printArgs->out( printArgs, "#", 1 );
		sprintf( buf, "%p", (void*) ((pointer_t*)kid->tree)->value );
		printArgs->out( printArgs, buf, strlen(buf) );
	}
	else if ( kid->tree->id == LEL_ID_STR ) {
		printStr( printArgs, ((str_t*)kid->tree)->value );
	}
//	else if ( kid->tree->id == LEL_ID_STREAM ) {
//		char buf[INT_SZ];
//		printArgs->out( printArgs, "#", 1 );
//		sprintf( buf, "%p", (void*) ((stream_t*)kid->tree)->in->file );
//		printArgs->out( printArgs, buf, strlen(buf) );
//	}
	else if ( kid->tree->tokdata != 0 && 
			stringLength( kid->tree->tokdata ) > 0 )
	{
		printArgs->out( printArgs, stringData( kid->tree->tokdata ), 
				stringLength( kid->tree->tokdata ) );
	}
}


void colm_print_null( program_t *prg, tree_t **sp,
		struct colm_print_args *args, kid_t *parent, kid_t *kid )
{
}

void openTreeXml( program_t *prg, tree_t **sp, struct colm_print_args *args,
		kid_t *parent, kid_t *kid )
{
	/* Skip the terminal that is for forcing trailing ignores out. */
	if ( kid->tree->id == 0 )
		return;

	struct lang_el_info *lelInfo = prg->rtd->lelInfo;

	/* List flattening: skip the repeats and lists that are a continuation of
	 * the list. */
	if ( parent != 0 && parent->tree->id == kid->tree->id && kid->next == 0 &&
			( lelInfo[parent->tree->id].repeat || lelInfo[parent->tree->id].list ) )
	{
		return;
	}

	const char *name = lelInfo[kid->tree->id].xmlTag;
	args->out( args, "<", 1 );
	args->out( args, name, strlen( name ) );
	args->out( args, ">", 1 );
}

void printTermXml( program_t *prg, tree_t **sp, struct colm_print_args *printArgs, kid_t *kid )
{
	//kid_t *child;

	/*child = */ treeChild( prg, kid->tree );
	if ( kid->tree->id == LEL_ID_PTR ) {
		char ptr[32];
		sprintf( ptr, "%p\n", (void*)((pointer_t*)kid->tree)->value );
		printArgs->out( printArgs, ptr, strlen(ptr) );
	}
	else if ( kid->tree->id == LEL_ID_STR ) {
		head_t *head = (head_t*) ((str_t*)kid->tree)->value;

		xmlEscapeData( printArgs, (char*)(head->data), head->length );
	}
	else if ( 0 < kid->tree->id && kid->tree->id < prg->rtd->firstNonTermId &&
			kid->tree->id != LEL_ID_IGNORE &&
			kid->tree->tokdata != 0 && 
			stringLength( kid->tree->tokdata ) > 0 )
	{
		xmlEscapeData( printArgs, stringData( kid->tree->tokdata ), 
				stringLength( kid->tree->tokdata ) );
	}
}


void closeTreeXml( program_t *prg, tree_t **sp, struct colm_print_args *args, kid_t *parent, kid_t *kid )
{
	/* Skip the terminal that is for forcing trailing ignores out. */
	if ( kid->tree->id == 0 )
		return;

	struct lang_el_info *lelInfo = prg->rtd->lelInfo;

	/* List flattening: skip the repeats and lists that are a continuation of
	 * the list. */
	if ( parent != 0 && parent->tree->id == kid->tree->id && kid->next == 0 &&
			( lelInfo[parent->tree->id].repeat || lelInfo[parent->tree->id].list ) )
	{
		return;
	}

	const char *name = lelInfo[kid->tree->id].xmlTag;
	args->out( args, "</", 2 );
	args->out( args, name, strlen( name ) );
	args->out( args, ">", 1 );
}

void printTreeCollect( program_t *prg, tree_t **sp, StrCollect *collect, tree_t *tree, int trim )
{
	struct colm_print_args printArgs = { collect, true, false, trim, &appendCollect, 
			&colm_print_null, &colm_print_term_tree, &colm_print_null };
	colm_print_tree_args( prg, sp, &printArgs, tree );
}

void printTreeFile( program_t *prg, tree_t **sp, FILE *out, tree_t *tree, int trim )
{
	struct colm_print_args printArgs = { out, true, false, trim, &appendFile, 
			&colm_print_null, &colm_print_term_tree, &colm_print_null };
	colm_print_tree_args( prg, sp, &printArgs, tree );
}

void printTreeFd( program_t *prg, tree_t **sp, int fd, tree_t *tree, int trim )
{
	struct colm_print_args printArgs = { (void*)((long)fd), true, false, trim, &appendFd,
			&colm_print_null, &colm_print_term_tree, &colm_print_null };
	colm_print_tree_args( prg, sp, &printArgs, tree );
}

void printXmlStdout( program_t *prg, tree_t **sp, tree_t *tree, int commAttr, int trim )
{
	struct colm_print_args printArgs = { stdout, commAttr, commAttr, trim, &appendFile, 
			&openTreeXml, &printTermXml, &closeTreeXml };
	colm_print_tree_args( prg, sp, &printArgs, tree );
}


