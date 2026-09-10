#include "Editor/EditorApplication.hpp"

int main()
{
	ApplicationSpecification specification;
	specification.Name = "Nova";

	EditorApplication application(specification);
	application.Run();

	return 0;
}
