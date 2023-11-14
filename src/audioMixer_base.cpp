#include "audioMixer_base.h"

audioMixerModule_base::audioMixerModule_base()
	:	active(false) {}

audioMixerModule_base::audioMixerModule_base(const outputParameter& outputCfg)
	:	active		(false),
		outputConfig(outputCfg) {}