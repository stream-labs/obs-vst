#include "VSTPlugin.h"

VSTPlugin::VSTPlugin(obs_source_t *sourceContext) : sourceContext{sourceContext} {
    int numChannels = 8;
    int blocksize = 512;

    inputs = (float **) malloc(sizeof(float **) * numChannels);
    outputs = (float **) malloc(sizeof(float **) * numChannels);
    for (int channel = 0; channel < numChannels; channel++) {
        inputs[channel] = (float *) malloc(sizeof(float *) * blocksize);
        outputs[channel] = (float *) malloc(sizeof(float *) * blocksize);
    }
}

void VSTPlugin::loadEffectFromPath(std::string path) {
    if (this->pluginPath.compare(path) != 0) {
        closeEditor();
        unloadEffect();
    }

    if (!effect) {
        pluginPath = path;
        effect = loadEffect();

        if (!effect) {
            //TODO: alert user of error
            return;
        }

        // Check plugin's magic number
        // If incorrect, then the file either was not loaded properly, is not a
        // real VST plugin, or is otherwise corrupt.
        if (effect->magic != kEffectMagic) {
            blog(LOG_WARNING, "VST Plugin's magic number is bad");
            return;
        }

        effect->dispatcher(effect, effOpen, 0, 0, NULL, 0.0f);

        // Set some default properties
        size_t sampleRate = audio_output_get_sample_rate(obs_get_audio());
        effect->dispatcher(effect, effSetSampleRate, 0, 0, NULL, sampleRate);
        int blocksize = 512;
        effect->dispatcher(effect, effSetBlockSize, 0, blocksize, NULL, 0.0f);

        effect->dispatcher(effect, effMainsChanged, 0, 1, 0, 0);

        effectReady = true;

        openEditor();
    }
}

void silenceChannel(float **channelData, int numChannels, long numFrames) {
    for(int channel = 0; channel < numChannels; ++channel) {
        for(long frame = 0; frame < numFrames; ++frame) {
            channelData[channel][frame] = 0.0f;
        }
    }
}

obs_audio_data* VSTPlugin::process(struct obs_audio_data *audio) {
    if (effect && effectReady) {
        silenceChannel(outputs, 8, audio->frames);

        float *adata[8] = {(float *) audio->data[0], (float *) audio->data[1],
                           (float *) audio->data[2], (float *) audio->data[3],
                           (float *) audio->data[4], (float *) audio->data[5],
                           (float *) audio->data[6], (float *) audio->data[7]};

        effect->processReplacing(effect, adata, outputs, audio->frames);

        for (size_t c = 0; c < 8; c++) {
            if (audio->data[c]) {
                for (size_t i = 0; i < audio->frames; i++) {
                    adata[c][i] = outputs[c][i];
                }
            }
        }
    }

    return audio;
}

void VSTPlugin::unloadEffect() {
    effectReady = false;

    if (effect) {
        effect->dispatcher(effect, effMainsChanged, 0, 0, 0, 0);
        effect->dispatcher(effect, effClose, 0, 0, NULL, 0.0f);
    }

    effect = NULL;

    unloadLibrary();
}

void VSTPlugin::openEditor() {
    if (effect && !editorWidget) {
        editorWidget = new EditorWidget(0, this);

        editorWidget->buildEffectContainer(effect);

        editorWidget->show();
    }
}

void VSTPlugin::closeEditor() {
    if (effect) {
        effect->dispatcher(effect, effEditClose, 0, 0, 0, 0);
    }
    if (editorWidget) {
        editorWidget->close();

        delete editorWidget;
        editorWidget = NULL;
    }
}

intptr_t VSTPlugin::hostCallback( AEffect *effect, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt) {
	intptr_t result = 0;

	// Filter idle calls...
	bool filtered = false;
	if (opcode == audioMasterIdle) {
		static bool wasIdle = false;
		if (wasIdle)
			filtered = true;
		else {
			printf("(Future idle calls will not be displayed!)\n");
			wasIdle = true;
		}
	}

	switch (opcode) {
	case audioMasterSizeWindow:
		// index: width, value: height
		if (editorWidget) {
			editorWidget->handleResizeRequest(index, value);
		}
		return 0;
		break;
	}

	return result;
}

std::string VSTPlugin::getChunk() {
    void *buf = NULL;

    intptr_t chunkSize = effect->dispatcher(effect, effGetChunk, 1, 0, &buf, 0.0);

    QByteArray data = QByteArray((char*)buf, chunkSize);
    return QString(data.toBase64()).toStdString();
}

void VSTPlugin::setChunk(std::string data) {
    QByteArray base64Data = QByteArray(data.c_str(), data.length());
    QByteArray chunkData = QByteArray::fromBase64(base64Data);

    void *buf = NULL;

    buf = chunkData.data();
    effect->dispatcher(effect, effSetChunk, 0, chunkData.length(), buf, 0);

}