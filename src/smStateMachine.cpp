#include <IwDebug.h>
#include <simplemenu.h>
#include <smStateMachine.h>

namespace SimpleMenu
{
	int initStateMachineCounter = 0;
	TsmManagedList<CsmState>* g_smStateMachineStack;

	class CsmStateAction
	{
	public:
		virtual ~CsmStateAction() {}
		virtual void Perform() {}
	};

	TsmManagedList<CsmStateAction>* g_smStateMachineActionQueue;

	class CsmStatePushAction: public CsmStateAction
	{
		CsmState* state;
	public:
		CsmStatePushAction(CsmState* s) {state = s;}
		virtual void Perform() {
			g_smStateMachineStack->push_back(state);
		}
	};
	class CsmStatePopAction: public CsmStateAction
	{
		virtual void Perform() {
			g_smStateMachineStack->back()->Release();
			g_smStateMachineStack->pop_back();
		}
	};
	class CsmStateCloseAllAction: public CsmStateAction
	{
		virtual void Perform() {
			while (!g_smStateMachineStack->empty())
			{
				g_smStateMachineStack->back()->Release();
				g_smStateMachineStack->pop_back();
			}
		}
	};
	void smStateMachinePushAction(CsmStateAction*a)
	{
		g_smStateMachineActionQueue->push_back(a);
	};
}

using namespace SimpleMenu;

//Get scriptable class declaration
CsmScriptableClassDeclaration* CsmStateMachine::GetClassDescription()
{
	static  TsmScriptableClassDeclaration<CsmStateMachine> d (0, "CsmStateMachine",
		ScriptTraits::Method("CloseAll", &CsmStateMachine::CloseAll),
		ScriptTraits::Method("Close", &CsmStateMachine::Close),
		ScriptTraits::Method("OpenMenuAtGroup", &CsmStateMachine::OpenMenuAtGroup),
		ScriptTraits::Method("Alert", &CsmStateMachine::Alert),
			0);
	return &d;
}
void CsmStateMachine::CloseAll() { smStateMachinePushAction(new CsmStateCloseAllAction()); }
void CsmStateMachine::Close() { smStateMachinePushAction(new CsmStatePopAction()); }
void CsmStateMachine::OpenMenuAtGroup(const char*g) { smStateMachinePushAction(new CsmStatePushAction(new CsmGroupMenuState(g,0))); }
void CsmStateMachine::Alert(const char*h,const char*t) { 
	IwAssertMsg(SM,false,("%s\n%s",h,t));
}

void SimpleMenu::smStateMachineInit()
{
	++initStateMachineCounter;
	if (initStateMachineCounter != 1)
		return;

	g_smStateMachineStack = new TsmManagedList<CsmState>();
	g_smStateMachineActionQueue = new TsmManagedList<CsmStateAction>();
	//#ifdef IW_BUILD_RESOURCES
	//IwGetResManager()->AddHandler(new CsmStateMachineScriptResHandler);
	//#endif

	//IW_CLASS_REGISTER(CsmStateMachineState);
	smRegisterClass(CsmStateMachine::GetClassDescription());

	smInit();
}

void SimpleMenu::smStateMachineTerminate()
{
	--initStateMachineCounter;
	if (initStateMachineCounter < 0)
		IwAssertMsg(SIMPLEMENU,false,("smStateMachineTerminate doesn't match smStateMachineInit"));
	if (initStateMachineCounter != 0)
		return;

	if (g_smStateMachineStack)
	{
		while (!g_smStateMachineStack->empty())
		{
			g_smStateMachineStack->back()->Release();
			g_smStateMachineStack->pop_back();
		}
		delete g_smStateMachineStack;
		g_smStateMachineStack = 0;
	}
	if (g_smStateMachineActionQueue)
	{
		g_smStateMachineActionQueue->Delete();
		delete g_smStateMachineActionQueue;
		g_smStateMachineActionQueue = 0;
	}

	smTerminate();
}

void SimpleMenu::smStateMachinePush(CsmState* s)
{
	g_smStateMachineActionQueue->push_back(new CsmStatePushAction(s));
}
void SimpleMenu::smStateMachinePop()
{
	g_smStateMachineActionQueue->push_back(new CsmStatePopAction());
}
void smStateMachineUpdate(int ms)
{
	CsmState* s = smStateMachinePeek();
	if (!s) return;
	s->Update(ms);
}
void smStateMachineRender()
{
	CsmState* s = smStateMachinePeek();
	if (!s) return;
	s->Prepare();
	IwGxClear(IW_GX_DEPTH_BUFFER_F);
	//IwGxClear(IW_GX_COLOUR_BUFFER_F | IW_GX_DEPTH_BUFFER_F);
	
	s->Render();

	IwGxFlush();
	IwGxSwapBuffers();
}

void SimpleMenu::smStateMachineIterate(smStateMachineStateContext* context)
{
	switch (context->state)
	{
		case SMACTION_DO_ACTION_OR_MENU:
			{
				if (!g_smStateMachineActionQueue->empty())
				{
					context->state = SMACTION_FADE_OUT;
					smStateMachineIterate(context);
				} 
				else if (!g_smStateMachineStack->empty()) 
				{
					smStateMachineUpdate(context->ms);
					smStateMachineRender();
				}
				break;
			}
		case SMACTION_RELEASE:
			{
				if (smStateMachinePeek()) smStateMachinePeek()->Release();
				context->prev = clock();
				context->state = SMACTION_DO_ACTION_OR_MENU;
				if (!g_smStateMachineActionQueue->empty())
				{
					for (size_t i=0; i<g_smStateMachineActionQueue->size(); ++i)
						(*g_smStateMachineActionQueue)[i]->Perform();
					g_smStateMachineActionQueue->Delete();
					context->state = SMACTION_LOAD;
				}
				smStateMachineIterate(context);
				break;
			}
		case SMACTION_FADE_OUT:
			{
				if (!smStateMachinePeek() || !smStateMachinePeek()->FadeOut(context->ms))
				{
					context->state = SMACTION_RELEASE;
					context->ms =0;
					smStateMachineIterate(context);
				}
				else
				{
					smStateMachineUpdate(context->ms);
					smStateMachineRender();
				}
				break;
			}
		case SMACTION_LOAD:
			{
				if (smStateMachinePeek()) smStateMachinePeek()->Load(context->inputQueue);
				context->prev = clock();
				context->state = SMACTION_FADE_IN;
				smStateMachineIterate(context);
				break;
			}
		case SMACTION_FADE_IN:
			{
				if (!smStateMachinePeek() || !smStateMachinePeek()->FadeIn(context->ms))
				{
					context->state = SMACTION_DO_ACTION_OR_MENU;
					context->ms = 0;
					smStateMachineIterate(context);
				}
				else
				{
					smStateMachineUpdate(context->ms);
					smStateMachineRender();
				}
				break;
			}
	}
}
void SimpleMenu::smStateMachineLoop(CsmInputFilter* input)
{
	CsmInputQueue* inputQueue = input->PushQueue();

	smStateMachineStateContext context;
	
	clock_t cur;
	cur = context.prev = clock();
	context.state = SMACTION_DO_ACTION_OR_MENU;
	context.inputQueue = inputQueue;
	
	for (;!g_smStateMachineActionQueue->empty() || !g_smStateMachineStack->empty();)
	{
		cur = clock();
		context.ms = (1000/30)-(cur-context.prev);
		if (context.ms < 0) context.ms = 0;

		s3eDeviceYield(context.ms);

		s3eKeyboardUpdate();
		s3ePointerUpdate();

		cur = clock();
		context.ms = (cur-context.prev); if (context.ms > 1000) context.ms = 1000; //Limit to 1 FPS
		context.prev = cur;

		if (s3eDeviceCheckQuitRequest())
			g_smStateMachineActionQueue->push_back(new CsmStateCloseAllAction());

		smStateMachineIterate(&context);
	}
	input->PopQueue(inputQueue);
}

CsmState* SimpleMenu::smStateMachinePeek()
{
	if (!g_smStateMachineStack)
		return 0;
	if (g_smStateMachineStack->empty())
		return 0;
	return g_smStateMachineStack->back();
}
// -----------------------
CsmMenuState::CsmMenuState()
{
	menu = 0;
}
CsmMenuState::~CsmMenuState()
{
}

bool CsmMenuState::FadeIn(int ms)
{
	if (!menu)
		return false;

	IwDebugTraceLinePrintf("FadeIn(%d)",ms);

	iwfixed prevT = menu->GetTransition() + IW_GEOM_ONE*ms/100;
	if (prevT > 0) prevT = 0;
	menu->SetTransition(prevT);
	return (prevT < 0);
}
bool CsmMenuState::FadeOut(int ms)
{
	if (!menu)
		return false;
	iwfixed prevT = menu->GetTransition() + IW_GEOM_ONE*ms/100;
	if (prevT > IW_GEOM_ONE) prevT = IW_GEOM_ONE;
	menu->SetTransition(prevT);
	return (prevT < IW_GEOM_ONE);
}
void CsmMenuState::Update(int ms)
{
	if (!menu)
		return;
	menu->Update(ms);
}
void CsmMenuState::Prepare()
{
	if (!menu)
		return;
	menu->Prepare();
}
void CsmMenuState::Render()
{
	if (!menu)
		return;
	menu->Render();
}


// -----------------------
CsmGroupMenuState::CsmGroupMenuState()
{
	group = 0;
}
CsmGroupMenuState::CsmGroupMenuState(const char* _groupName, const char* _menuName)
{
	if (_groupName)
		groupName = _groupName;
	if (_menuName)
		menuName = _menuName;
}
CsmGroupMenuState::~CsmGroupMenuState()
{
}
void CsmGroupMenuState::Load(CsmInputQueue* inputQueue)
{
	if (!groupName.empty())
	{
		group =  IwGetResManager()->LoadGroup(groupName.c_str(), true);
		if (!group)
		{
			CsmStateMachine::Close();
			return;
		}
	}
	if (menuName.empty())
	{
		CIwResList* list = group->GetListNamed("CsmMenu", IW_RES_PERMIT_NULL_F | IW_RES_IGNORE_SHARED_F | IW_RES_IGNORE_CHILDREN_F);
		if (list)
		{
			if (list->m_Resources.GetSize() > 0)
			{
				menu = (CsmMenu*)list->m_Resources[0];
			}
		}
	}
	else
	{
		menu = (CsmMenu*)group->GetResNamed(menuName.c_str(), "CsmMenu", IW_RES_PERMIT_NULL_F | IW_RES_IGNORE_SHARED_F | IW_RES_IGNORE_CHILDREN_F);
	}
	if (menu)
	{
		menu->Initialize(smGetDefaultScriptProvider());
		inputQueue->RegisterReceiver(menu);
		menu->SetTransition(-IW_GEOM_ONE);
	}
}
void CsmGroupMenuState::Release()
{
	if (menu)
	{
		((CsmInputQueue*)((IsmInputReciever*)menu)->GetListConainer())->UnRegisterReceiver(menu);
	}
	if (group)
	{
		IwGetResManager()->DestroyGroup(group);
	}
	menu = 0;
	group = 0;
}
