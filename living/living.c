/*
**	$Filename: living/living.c
**	$Release:  0
**	$Revision: 0.1
**	$Date:     13-04-94
**
**	"sprite" functions for "Der Clou!"
**
**   (c) 1994 ...and avoid panic by, H. Gaberschek
**	    All Rights Reserved.
**
*/
/****************************************************************************
  Portions copyright (c) 2005 Vasco Alexandre da Silva Costa

  Please read the license terms contained in the LICENSE and
  publiclicensecontract.doc files which should be contained with this
  distribution.
 ****************************************************************************/

#include "living/living.h"
#include "living/living.ph"

void livInit(uword us_VisLScapeX, uword us_VisLScapeY,
	     uword us_VisLScapeWidth, uword us_VisLScapeHeight,
	     uword us_TotalLScapeWidth, uword us_TotalLScapeHeight,
	     ubyte uch_FrameCount, U32 ul_StartArea)
{
    gSpriteControl = TCAllocMem(sizeof(*gSpriteControl), 0);

    gSpriteControl->p_Livings = CreateList();
    gSpriteControl->p_Template = CreateList();

    gSpriteControl->us_VisLScapeX = us_VisLScapeX;
    gSpriteControl->us_VisLScapeY = us_VisLScapeY;

    gSpriteControl->us_VisLScapeWidth = us_VisLScapeWidth;
    gSpriteControl->us_VisLScapeHeight = us_VisLScapeHeight;

    gSpriteControl->us_TotalLScapeWidth = us_TotalLScapeWidth;
    gSpriteControl->us_TotalLScapeHeight = us_TotalLScapeHeight;

    livSetActivAreaId(ul_StartArea);

    gSpriteControl->uch_FrameCount = uch_FrameCount;

    livLoadTemplates();		/* load all anim phases (frames) */

    livLoadLivings();		/* and the corresponding creatures */

    livSetPlayMode(LIV_PM_NORMAL);
}

void livDone(void)
{
    if (gSpriteControl) {
	if (gSpriteControl->p_Livings) {
	    NODE *node;

	    for (node = (NODE *) LIST_HEAD(gSpriteControl->p_Livings); NODE_SUCC(node);
		 node = (NODE *) NODE_SUCC(node))
		livRem((struct Living *) node);

	    RemoveList(gSpriteControl->p_Livings);
	    gSpriteControl->p_Livings = NULL;
	}

	if (gSpriteControl->p_Template) {
	    NODE *node;

	    for (node = (NODE *) LIST_HEAD(gSpriteControl->p_Template); NODE_SUCC(node);
		 node = (NODE *) NODE_SUCC(node))
		livRemTemplate((struct AnimTemplate *) node);

	    RemoveList(gSpriteControl->p_Template);
	    gSpriteControl->p_Template = NULL;
	}

	TCFreeMem(gSpriteControl, sizeof(*gSpriteControl));

	gSpriteControl = NULL;
    }
}

void livSetActivAreaId(U32 areaId)
{
    if (gSpriteControl)
	gSpriteControl->ul_ActivAreaId = areaId;
}

void livLivesInArea(char *uch_Name, U32 areaId)
{
    struct Living *liv = livGet(uch_Name);

    if (liv)
	liv->ul_LivesInAreaId = areaId;
}

void livRefreshAll(void)
{
    livDoAnims(0, 0);
}

void livSetAllInvisible(void)
{
    struct Living *liv;

    if (gSpriteControl) {
	for (liv = (struct Living *) LIST_HEAD(gSpriteControl->p_Livings); NODE_SUCC(liv);
	     liv = (struct Living *) NODE_SUCC(liv))
	    livHide(liv);
    }
}

void livSetPlayMode(U32 playMode)
{
    gSpriteControl->ul_SprPlayMode = playMode;

    if (gSpriteControl->ul_SprPlayMode & LIV_PM_NORMAL) {
	gSpriteControl->uch_FirstFrame = 0;
	gSpriteControl->uch_LastFrame = gSpriteControl->uch_FrameCount;
	gSpriteControl->ch_PlayDirection = 1;
    }

    if (gSpriteControl->ul_SprPlayMode & LIV_PM_REVERSE) {
	gSpriteControl->uch_FirstFrame = gSpriteControl->uch_FrameCount - 1;
	gSpriteControl->uch_LastFrame = (ubyte) - 1;
	gSpriteControl->ch_PlayDirection = -1;
    }
}

U32 livWhereIs(char *uch_Name)
{
    struct Living *liv = livGet(uch_Name);
    U32 loc = 0;

    if (liv)
	loc = liv->ul_LivesInAreaId;

    return loc;
}

void livSetPos(char *uch_Name, uword XPos, uword YPos)
{
    struct Living *liv = livGet(uch_Name);

    if (liv) {
	liv->us_XPos = XPos;
	liv->us_YPos = YPos;
    }
}

void livAnimate(char *uch_Name, ubyte uch_Action, word s_XSpeed, word s_YSpeed)
{
    struct Living *liv = livGet(uch_Name);

    if (liv) {
	liv->uch_Status = LIV_ENABLED;

	liv->uch_OldAction = liv->uch_Action;

	liv->uch_Action = uch_Action;
	liv->s_XSpeed = s_XSpeed;
	liv->s_YSpeed = s_YSpeed;

	if (((ubyte) liv->ch_CurrFrameNr == gSpriteControl->uch_LastFrame))
	    liv->ch_CurrFrameNr = (char) gSpriteControl->uch_FirstFrame;
    }
}

void livTurn(char *puch_Name, ubyte uch_Status)
{
    struct Living *liv = livGet(puch_Name);

    if (liv)
	liv->uch_Status = uch_Status;	/* enable or disable */
}

void livStopAll(void)
{
    struct Living *liv;

    for (liv = (struct Living *) LIST_HEAD(gSpriteControl->p_Livings);
	 NODE_SUCC(liv); liv = (struct Living *) NODE_SUCC(liv)) {
	if (liv->uch_Status == LIV_ENABLED)
	    livAnimate(NODE_NAME(liv), ANM_STAND, 0, 0);
    }
}

static void livCorrectViewDirection(struct Living *liv)
{
    char *name = NODE_NAME(liv->p_OriginTemplate);

    if (liv->uch_Action <= ANM_MOVE_LEFT)
	liv->uch_ViewDirection = liv->uch_Action;
    else {
	if (!strcmp(name, LIV_TEMPL_BULLE_NAME)
	    && (liv->uch_Action == ANM_WORK_CONTROL))
	    liv->uch_ViewDirection = ANM_MOVE_DOWN;
    }
}

void livPrepareAnims(void)
{
}

void livDoAnims(ubyte uch_Play, ubyte uch_Move)
{
    struct Living *liv;

    livPrepareAnims();

    lsDoScroll();

    for (liv = (struct Living *) LIST_HEAD(gSpriteControl->p_Livings);
	 NODE_SUCC(liv); liv = (struct Living *) NODE_SUCC(liv)) {
	if (liv->uch_Status == LIV_ENABLED) {
	    if (uch_Move) {
		liv->us_XPos += liv->s_XSpeed;
		liv->us_YPos += liv->s_YSpeed;
	    }

	    /*
	     * die View Direction mu� hier gesetzt werden und NICHT
	     * in AnimateLiving, da die Aktion in AnimateLiving nicht
	     * stattfinden mu�, die ViewDirection zwischenzeitlich
	     * aber eine falschen Wert annimt, was z.B. im Planing
	     * zu Fehler f�hrt!
	     * mu� in jedem Fall geschehen, auch wenn Maxi unsichtbar ist
	     */

	    livCorrectViewDirection(liv);

	    if (livIsVisible(liv)) {
		livShow(liv);

		/* Action != ANM_STAND -> shitty exception because Marx
                   didn't provide a standing anim */
		if ((uch_Play) && (liv->uch_Action != ANM_STAND))
		    liv->ch_CurrFrameNr += (char) gSpriteControl->ch_PlayDirection;

		if ((ubyte) liv->ch_CurrFrameNr == gSpriteControl->uch_LastFrame)
		    livAnimate(NODE_NAME(liv), ANM_STAND, 0, 0);
	    } else
		livHide(liv);
	}
    }
}

void livSetVisLScape(uword us_VisLScapeX, uword us_VisLScapeY)
{
    gSpriteControl->us_VisLScapeX = us_VisLScapeX;
    gSpriteControl->us_VisLScapeY = us_VisLScapeY;
}

uword livGetXPos(char *Name)
{
    struct Living *liv = livGet(Name);

    return (liv->us_XPos);
}

uword livGetYPos(char *Name)
{
    struct Living *liv = livGet(Name);

    return (liv->us_YPos);
}

ubyte livGetViewDirection(char *uch_Name)
{
    struct Living *liv = livGet(uch_Name);

    return (liv->uch_ViewDirection);
}

ubyte livGetOldAction(char *uch_Name)
{
    struct Living *liv = livGet(uch_Name);

    return (liv->uch_OldAction);
}

ubyte livIsPositionInViewDirection(uword us_GXPos, uword us_GYPos,
				   uword us_XPos, uword us_YPos,
				   ubyte uch_ViewDirection)
{
    ubyte InDirection = 1;

    switch (uch_ViewDirection) {
    case ANM_MOVE_LEFT:
	if (us_XPos > us_GXPos)
	    InDirection = 0;
	break;
    case ANM_MOVE_RIGHT:
	if (us_XPos < us_GXPos)
	    InDirection = 0;
	break;
    case ANM_MOVE_DOWN:
	if (us_YPos < us_GYPos)
	    InDirection = 0;
	break;
    case ANM_MOVE_UP:
	if (us_YPos > us_GYPos)
	    InDirection = 0;
	break;
    default:
	InDirection = 0;
	break;
    }

    return InDirection;
}

ubyte livCanWalk(char *puch_Name)
{
    struct Living *liv = livGet(puch_Name);

    if (liv) {
	ubyte direction;

	switch (liv->uch_Action) {
	case ANM_MOVE_UP:
	    direction = LS_SCROLL_UP;
	    break;
	case ANM_MOVE_DOWN:
	    direction = LS_SCROLL_DOWN;
	    break;
	case ANM_MOVE_LEFT:
	    direction = LS_SCROLL_LEFT;
	    break;
	case ANM_MOVE_RIGHT:
	    direction = LS_SCROLL_RIGHT;
	    break;
	default:
	    return 0;
	}

	if (!
	    (lsIsCollision
	     (liv->us_XPos + liv->s_XSpeed, liv->us_YPos + liv->s_YSpeed,
	      direction)))
	    return 1;
    }

    return 0;
}

static struct Living *livGet(char *uch_Name)
{
    struct Living *liv = NULL;

    liv = (struct Living *) GetNode(gSpriteControl->p_Livings, uch_Name);

    if (!liv)
	ErrorMsg(Internal_Error, ERROR_MODULE_LIVING, 1);

    return (liv);
}

static void livAdd(char *uch_Name, char *uch_TemplateName, ubyte uch_XSize,
		   ubyte uch_YSize, word s_XSpeed, word s_YSpeed)
{
    struct Living *liv;
    struct AnimTemplate *tlt;

    liv = (struct Living *)
	CreateNode(gSpriteControl->p_Livings, sizeof(struct Living), uch_Name);

    liv->uch_XSize = uch_XSize;
    liv->uch_YSize = uch_YSize;

    tlt = liv->p_OriginTemplate =
	(struct AnimTemplate *) GetNode(gSpriteControl->p_Template, uch_TemplateName);

    liv->us_LivingNr = BobInit(tlt->us_Width, tlt->us_Height);

    liv->uch_OldAction = 0;
    liv->uch_Action = 0;

    liv->uch_ViewDirection = 0;

    liv->ch_CurrFrameNr = 0;

    liv->s_XSpeed = s_XSpeed;
    liv->s_YSpeed = s_YSpeed;

    liv->us_XPos = 0;
    liv->us_YPos = 0;

    liv->ul_LivesInAreaId = gSpriteControl->ul_ActivAreaId;

    liv->uch_Status = LIV_DISABLED;
}

static void livRem(struct Living *liv)
{
    BobDone(liv->us_LivingNr);
}

static void livLoadTemplates(void)
{
    uword cnt, i;
    char *line;
    LIST *l = CreateList();
    struct AnimTemplate *tlt;
    char pathname[DSK_PATH_MAX];

    dskBuildPathName(DISK_CHECK_FILE, TEXT_DIRECTORY, LIV_ANIM_TEMPLATE_LIST, pathname);

    if (!(cnt = ReadList(l, 0L, pathname)))
	ErrorMsg(Disk_Defect, ERROR_MODULE_LIVING, 3);

    for (i = 0; i < cnt; i++) {
	line = NODE_NAME(GetNthNode(l, i));

	tlt = (struct AnimTemplate *) CreateNode(gSpriteControl->p_Template,
						 sizeof(struct AnimTemplate),
						 txtGetKey(1, line));

	tlt->us_Width = (uword) txtGetKeyAsULONG(2, line);
	tlt->us_Height = (uword) txtGetKeyAsULONG(3, line);

	tlt->us_FrameOffsetNr = (uword) txtGetKeyAsULONG(4, line);
    }

    RemoveList(l);
}

static void livRemTemplate(struct AnimTemplate *tlt)
{
    /* dummy function */
}

static void livLoadLivings(void)
{
    uword cnt, i;
    char *line;
    LIST *l = CreateList();
    char pathname[DSK_PATH_MAX];

    dskBuildPathName(DISK_CHECK_FILE, TEXT_DIRECTORY, LIV_LIVINGS_LIST, pathname);

    if (!(cnt = ReadList(l, 0L, pathname)))
	ErrorMsg(Disk_Defect, ERROR_MODULE_LIVING, 2);

    for (i = 0; i < cnt; i++) {
	char name[TXT_KEY_LENGTH], template[TXT_KEY_LENGTH];

	line = NODE_NAME(GetNthNode(l, i));

	strcpy(name, txtGetKey(1, line));
	strcpy(template, txtGetKey(2, line));

	livAdd(name,
	       template,
	       (ubyte) txtGetKeyAsULONG(3, line),
	       (ubyte) txtGetKeyAsULONG(4, line),
	       (word) txtGetKeyAsULONG(5, line),
	       (word) txtGetKeyAsULONG(6, line));
    }

    RemoveList(l);
}

static void livHide(struct Living *liv)
{
    BobInVis(liv->us_LivingNr);
}

static void livShow(struct Living *liv)
{
    uword frameNr, action;
    uword srcX, srcY, offset;
    struct AnimTemplate *tlt = liv->p_OriginTemplate;

    /* shitty exception because Marx didn't provide a standing anim */
    if (liv->uch_Action == ANM_STAND) {
	action = liv->uch_ViewDirection;
	liv->ch_CurrFrameNr = 4;
    } else
	action = liv->uch_Action;

    frameNr = action * gSpriteControl->uch_FrameCount + liv->ch_CurrFrameNr;

    frameNr = frameNr + tlt->us_FrameOffsetNr;

    offset = frameNr * tlt->us_Width;

    srcY = (offset / LIV_COLL_WIDTH) * tlt->us_Height;
    srcX = (offset % LIV_COLL_WIDTH);

    if (BobSet(liv->us_LivingNr, liv->us_XPos, liv->us_YPos, srcX, srcY))
	BobVis(liv->us_LivingNr);
}

static ubyte livIsVisible(struct Living *liv)
{
    uword left, right, up, down;
    ubyte visible = 0;

    left = liv->us_XPos;
    right = left + liv->uch_XSize;
    up = liv->us_YPos;
    down = up + liv->uch_YSize;

    if (liv->ul_LivesInAreaId == gSpriteControl->ul_ActivAreaId)
	if (right > gSpriteControl->us_VisLScapeX)
	    if (left < (gSpriteControl->us_VisLScapeX + gSpriteControl->us_VisLScapeWidth))
		if (down > gSpriteControl->us_VisLScapeY)
		    if (up < (gSpriteControl->us_VisLScapeY + gSpriteControl->us_VisLScapeHeight))
			visible = 1;

    return (visible);
}
