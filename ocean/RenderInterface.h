#pragma once

class RenderInterface {
public:
	RenderInterface();
	virtual ~RenderInterface() = 0;

	virtual void render() = 0;

};

