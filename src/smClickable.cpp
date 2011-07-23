#include <IwTextParserITX.h>
#include <IwResManager.h>
#include <IwGx.h>
#include "SimpleMenu.h"
#include "smClickable.h"
#include "smMenu.h"

using namespace SimpleMenu;

namespace SimpleMenu
{
	
}

//Instantiate the default factory function for a named class 
IW_CLASS_FACTORY(CsmClickable);
//This macro is required within some source file for every class derived from CIwManaged. It implements essential functionality
IW_MANAGED_IMPLEMENT(CsmClickable);

//Constructor
CsmClickable::CsmClickable()
{

}
//Desctructor
CsmClickable::~CsmClickable()
{
}

//Reads/writes a binary file using @a IwSerialise interface.
void CsmClickable::Serialise ()
{
	CsmItem::Serialise();
	smSerialiseString(onClick);
}
void CsmClickable::Prepare(smItemContext* renderContext,int16 width)
{
	CsmItem::Prepare(renderContext,width);
}

//Render image on the screen surface
void CsmClickable::Render(smItemContext* renderContext)
{
	CsmItem::Render(renderContext);
}
uint32 CsmClickable::GetElementNameHash()
{
	static uint32 name = IwHashString("CLICKABLE");
	return name;
}
void CsmClickable::TouchReleased(smTouchContext* touchContext)
{
	SendLazyEvent(new CsmLazyClick(this));
}
#ifdef IW_BUILD_RESOURCES

//Parses from text file: parses attribute/value pair.
bool	CsmClickable::ParseAttribute(CIwTextParserITX* pParser, const char* pAttrName)
{
	if (!stricmp(pAttrName, "onclick"))
	{
		smReadString(pParser, &onClick);
		return true;
	}
	return CsmItem::ParseAttribute(pParser, pAttrName);
}

#endif