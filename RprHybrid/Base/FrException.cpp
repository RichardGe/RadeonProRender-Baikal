#include "FrException.h"

#include "RprHybrid/Node/FrNode.h"


FrException::FrException(const char* fileName, int line, int errorCode, std::string const& errorDesc , void* frNode ) throw()
    : errorCode_(errorCode)
    , errorDesc_(errorDesc)
{
	
	if ( frNode )
	{

		FrNode* nodefr = (FrNode*)frNode;
		FrNode* node_Context = nullptr;

		if ( nodefr->GetType() == NodeTypes::Context ) // if it's a context, we use it directly
		{
			node_Context = nodefr;
		}
		else // if it's not a context get the context from the node
		{
			node_Context = nodefr->GetProperty<FrNode*>(FR_NODE_CONTEXT);
		}

		if ( node_Context )
		{
			node_Context->SetProperty(RPR_CONTEXT_LAST_ERROR_MESSAGE,errorDesc + "  //// FREXCEPTION : FILE=" + std::string(fileName) + " LINE=" + std::to_string(line) + " ERROR=" + std::to_string(errorCode) + " ////");
		}
	}

}


FrException::FrException(int errorCode, std::string const& errorDesc ) throw()
    : errorCode_(errorCode)
    , errorDesc_(errorDesc)
{

}




