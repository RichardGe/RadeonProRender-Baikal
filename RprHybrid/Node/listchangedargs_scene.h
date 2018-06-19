
#include "FireSG/SceneGraph.h"
#include "RprHybrid/RadeonProRender.h"

class ListChangedArgs_Scene : public FireSG::ListChangedArgs
{
public:

	ListChangedArgs_Scene(FrNode* scene , Op op, void* item) : FireSG::ListChangedArgs(op,item)
	{
		m_scene = scene;
	}

	FrNode* m_scene;

};



