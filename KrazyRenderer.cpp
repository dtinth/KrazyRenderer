/**
 * KrazyRenderer: Renders a Krazy Rain song to .WAV file.
 */

#include <algorithm>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <map>
#include <set>

#define EXPORT_NOTES 0
using namespace std;
string inputname, dirname, notename, xnename, volname, outname;
FILE *fp;

template <class T>
void read(T &out, FILE *fp) {
	fread(&out, sizeof(T), 1, fp);
}

template <class T>
void read(T &out) {
	read(out, fp);
}

template <class T>
void write(T &out, FILE *fp) {
	fwrite(&out, sizeof(T), 1, fp);
}

void skip(size_t n, FILE *fp) {
	fseek(fp, n, SEEK_CUR);
}

void skip(size_t n) {
	skip(n, fp);
}

//-----------------------------------------------------------------

struct Sample {
	int numChannels;
	string filename;
	int16_t *sampleData;
	size_t samples;
	float volume;
	Sample(string);
	~Sample();
	bool loadFromWavFile(FILE *, string &);
	void load();
	int32_t getSample(size_t, int);
};

int32_t Sample::getSample(size_t time, int channel) {
	if (numChannels == 1) {
		return (int32_t)((float)sampleData[time] * volume);
	}
	return (int32_t)((float)sampleData[time * 2 + channel] * volume);
}

bool Sample::loadFromWavFile(FILE *fs, string &error) {
	
	{
		char sig[8];
		read(sig, fs);
		if (strncmp(sig, "RIFF", 4) != 0) {
			error = "Not a .WAV file";
			return false;
		}
		
		read(sig, fs);
		if (strncmp(sig, "WAVEfmt ", 8) != 0) {
			error = "Not a .WAV file";
			return false;
		}
	}
	
	size_t data;
	{
		int32_t size;
		read(size, fs);
		data = ftell(fs) + size;
	}
	
	{
		int16_t format, channels, bits;
		int32_t sampleRate;
		
		read(format, fs);
		read(channels, fs);
		read(sampleRate, fs);
		skip(6, fs);
		read(bits, fs);
		numChannels = channels;
		
		if (format != 1) {
			error = "Not a PCM file.";
			return false;
		}
		
		if (channels != 1 && channels != 2) {
			error = "Not a mono/stereo file.";
			return false;
		}
		
		if (sampleRate != 44100) {
			error = "Sample rate != 44.1khz";
			return false;
		}
		
		if (bits != 16) {
			error = "Not a 16-bit file.";
			return false;
		}
	}
	
	{
		char sig[4];
		fseek(fs, data, SEEK_SET);
		read(sig, fs);
		if (strncmp(sig, "data", 4) != 0) {
			error = "Data block not found!";
		}
	}
	
	{
		int32_t size;
		read(size, fs);
		sampleData = new int16_t[size / 2];
		samples = (size / 2) / numChannels;
		fread(sampleData, 2, size / 2, fs);
	}
	
	return true;
}

void Sample::load() {
	string wavFilename = filename.substr(0, filename.size() - 3) + "wav";
	FILE *fs = fopen((dirname + wavFilename).c_str(), "rb");
	printf("Loading %s...\n", wavFilename.c_str());
	if (fs == NULL) {
		printf("!!! %s can't be opened.\n", wavFilename.c_str());
		return;
	}
	string error;
	if (!loadFromWavFile(fs, error)) {
		printf("!!! %s can't be loaded: %s.\n", wavFilename.c_str(), error.c_str());
	}
	fclose(fs);
}

Sample::~Sample() {
	if (sampleData != NULL) {
		delete sampleData;
	}
}

Sample::Sample(string inFilename) {
	samples = -1;
	volume = 1;
	sampleData = NULL;
	filename = inFilename;
}

//-----------------------------------------------------------------

map<float, float> tempoChanges;
map<int16_t, Sample *> keysounds;
float initialTempo;

struct Note {
	float time;
	float length;
	int channel;
	int keysoundID;
};

float timeToSeconds(float time) {
	map<float, float>::iterator
		ub = tempoChanges.upper_bound(time),
		it = tempoChanges.begin();
	float
		seconds = 0.,
		tempo = initialTempo,
		lastTime = 0.;
	for (; it != ub; it ++) {
		if (time > it->first) {
			seconds += (it->first - lastTime) * 4.0 * 60.0 / tempo;
			lastTime = it->first;
			tempo = it->second;
		}
	}
	seconds += (time - lastTime) * 4.0 * 60.0 / tempo;
	return seconds;
}

void readBpmChanges() {
	int32_t count;
	skip(12);
	read(count);
	for (int i = 0; i < count; i ++) {
		int16_t measure;
		float positionInMeasure;
		float tempo;
		skip(1);
		read(measure);
		read(positionInMeasure);
		skip(3);
		read(tempo);
		tempoChanges[(float)measure + positionInMeasure] = tempo;
	}
}

void readNotes(vector<Note> &out) {
	int32_t count;
	skip(12);
	read(count);
	for (int i = 0; i < count; i ++) {
		int16_t measure;
		float positionInMeasure;
		int8_t channel;
		int16_t keysoundID;
		float holdLength;
		skip(1);
		read(measure);
		read(positionInMeasure);
		read(channel);
		read(keysoundID);
		read(holdLength);
		out.push_back((Note){ (float)measure + positionInMeasure, holdLength, channel, keysoundID });
	}
}

void readKeysoundDefinition() {
	int32_t count;
	read(count);
	for (int i = 0; i < count; i ++) {
		int16_t keysoundID;
		int32_t length;
		char *filenameChars;
		string filename;
		read(keysoundID);
		skip(2);
		read(length);
		filenameChars = new char[length + 1];
		fread(filenameChars, 1, length, fp);
		filenameChars[length] = 0;
		filename = filenameChars;
		delete filenameChars;
		keysounds[keysoundID] = new Sample(filename);
	}
}

void updateMinMeasureFromNotes(vector<Note> &in, int &out) {
	vector<Note>::iterator it = in.begin();
	for (; it != in.end(); it ++) {
		if ((int)it->time < out) {
			out = (int)it->time;
		}
	}
}

//-----------------------------------------------------------------

struct SoundInstance {
	Sample *sample;
	size_t start;
	SoundInstance(Sample *, size_t);
	bool stillPlaying(size_t);
	int32_t getSample(size_t, int);
};

bool SoundInstance::stillPlaying(size_t now) {
	return now < start + sample->samples;
}

int32_t SoundInstance::getSample(size_t now, int channel) {
	return sample->getSample(now - start, channel);
}

SoundInstance::SoundInstance(Sample *a, size_t b) {
	sample = a;
	start = b;
}

//-----------------------------------------------------------------

struct SoundEvent {
	size_t time;
	int16_t keysoundID;
	SoundEvent(float, int16_t);
};

SoundEvent::SoundEvent(float a, int16_t b) {
	time = a * 44100;
	keysoundID = b;
}

vector<SoundEvent> soundEvents;

void writeSound(map<int16_t, float> &volumeMap) {
	
	vector<SoundInstance *> instances;
	vector<SoundEvent>::iterator eit;
	map<int16_t, Sample *>::iterator sit;
	vector<SoundInstance *>::iterator iit;
	
	{
		map<int16_t, float>::iterator vmit;
		int i = 0;
		for (sit = keysounds.begin(); sit != keysounds.end(); sit ++) {
			printf("(%d/%lu) [%d]: ", ++i, keysounds.size(), sit->first);
			sit->second->load();
			vmit = volumeMap.find(sit->first);
			if (vmit != volumeMap.end()) {
				sit->second->volume = vmit->second;
			}
		}
	}

	int32_t maxAmplitude = 0, maxAmpNow = 0;
	size_t sampleNumber = 0;
	size_t lastNote = 0;
	for (eit = soundEvents.begin(); eit != soundEvents.end(); eit ++) {
		size_t time = eit->time;
		sit = keysounds.find(eit->keysoundID);
		if (sit != keysounds.end()) {
			time += sit->second->samples;
		}
		if (time > lastNote) {
			lastNote = time;
		}
	}
	
	vector<int32_t> output;
	output.reserve(lastNote * 2 + 44100);
	
	eit = soundEvents.begin();
	vector<SoundInstance *> nextInstances;
	
	for (; eit != soundEvents.end() || instances.size() > 0; sampleNumber ++) {
		int32_t left = 0, right = 0, amplitude;
		if (sampleNumber % 44100 == 0) {
			printf("Rendering: %3lu sec: ", sampleNumber / 44100);
			for (int i = 0; i < maxAmpNow; i += 4096) {
				putchar(']');
			}
			printf("\n");
			maxAmpNow = 0;
		}
		if (eit != soundEvents.end()) {
			if (sampleNumber >= eit->time) {
				sit = keysounds.find(eit->keysoundID);
				if (sit != keysounds.end()) {
					instances.push_back(new SoundInstance(sit->second, sampleNumber));
					//printf("%10lu: [%d] %s start\n", sampleNumber, eit->keysoundID, sit->second->filename.c_str());
				}
				eit ++;
			}
		}
		nextInstances.clear();
		for (iit = instances.begin(); iit != instances.end(); iit++) {
			if ((*iit)->stillPlaying(sampleNumber)) {
				left  += (*iit)->getSample(sampleNumber, 0);
				right += (*iit)->getSample(sampleNumber, 1);
				nextInstances.push_back(*iit);
			} else {
				delete *iit;
				//printf("%10lu: %s end\n", sampleNumber, (*iit)->sample->filename.c_str());
			}
		}
		instances = nextInstances;
		amplitude = max(abs(left), abs(right));
		if (amplitude > maxAmplitude) {
			maxAmplitude = amplitude;
		}
		if (amplitude > maxAmpNow) {
			maxAmpNow = amplitude;
		}
		output.push_back(left);
		output.push_back(right);
	}
	
	printf("Max Amplitude: %d\n", maxAmplitude);
	
	{
		FILE *fo = fopen(outname.c_str(), "wb");
		size_t size = output.size();
		{
			int32_t subchunk1Size = 16;
			int32_t subchunk2Size = (int32_t)size * 2;
			int32_t chunkSize = 36 + subchunk2Size;
			int16_t audioFormat = 1;
			int16_t numChannels = 2;
			int32_t	sampleRate = 44100;
			int32_t	byteRate = sampleRate * numChannels * 2;
			int16_t	blockAlign = numChannels * 2;
			int16_t	bitsPerSample = 16;
			fwrite("RIFF", 1, 4, fo);
			write(chunkSize, fo);
			fwrite("WAVEfmt ", 1, 8, fo);
			write(subchunk1Size, fo);
			write(audioFormat, fo);
			write(numChannels, fo);
			write(sampleRate, fo);
			write(byteRate, fo);
			write(blockAlign, fo);
			write(bitsPerSample, fo);
			fwrite("data", 1, 4, fo);
			write(subchunk2Size, fo);
		}
		int percentage = 0;
		for (size_t i = 0; i < size; i ++) {
			int pct = (float)i * (100.0 / size);
			if (pct > percentage) {
				percentage = pct;
				printf("Writing: %d%%\n", pct);
			}
			int16_t value = (float)output[i] * (32766.0 / maxAmplitude);
			fwrite(&value, 2, 1, fo);
		}
		fclose(fo);
	}
		
	printf("Unloading sounds...\n");
	for (sit = keysounds.begin(); sit != keysounds.end(); sit ++) {
		delete sit->second;
	}
	
	printf("Finished!\n");
	
}

void addEventsFromNotes(vector<Note> &in, float shift) {
	vector<Note>::iterator it = in.begin();
	for (; it != in.end(); it ++) {
		soundEvents.push_back(SoundEvent(timeToSeconds(it->time) - shift, it->keysoundID));
	}
}

int eventcmp(const SoundEvent &a, const SoundEvent &b) {
	return a.time < b.time;
}

//-----------------------------------------------------------------

typedef short NotesExportData[192];

struct NotesExportChannel {
	NotesExportData notes;
	NotesExportChannel() {
		for (int i = 0; i < 192; i ++) {
			notes[i] = 0;
		}
	}
};

struct NotesExportMeasure {
	map<short, NotesExportChannel *> channels;
	NotesExportChannel *getChannel(short channel) {
		if (channels.find(channel) == channels.end()) {
			return (channels[channel] = new NotesExportChannel);
		}
		return channels[channel];
	}
	~NotesExportMeasure() {
		map<short, NotesExportChannel *>::iterator it;
		for (it = channels.begin(); it != channels.end(); it ++) {
			delete it->second;
		}
	}
};

struct NotesExportFile {
	map<short, NotesExportMeasure *> measures;
	NotesExportMeasure *getMeasure(short measure) {
		if (measures.find(measure) == measures.end()) {
			return (measures[measure] = new NotesExportMeasure);
		}
		return measures[measure];
	}
	short *getNote(short channel, float time) {
		long bmsTime = (int)(time * 192);
		short measure = bmsTime / 192;
		short index = bmsTime % 192;
		return &getMeasure(measure)->getChannel(channel)->notes[index];
	}
	~NotesExportFile() {
		map<short, NotesExportMeasure *>::iterator it;
		for (it = measures.begin(); it != measures.end(); it ++) {
			delete it->second;
		}
	}
	void addNotesFromNotes(vector<Note> &, bool);
	void addNoteFromNote(Note &, bool);
};

void NotesExportFile::addNotesFromNotes(vector<Note> &notes, bool isAuto) {
	vector<Note>::iterator it = notes.begin();
	for (; it != notes.end(); it ++) {
		addNoteFromNote(*it, isAuto);
	}
}

void NotesExportFile::addNoteFromNote(Note &note, bool isAuto) {
	static short channelMap[8] = {16, 11, 12, 13, 14, 15, 18, 19};
	if (isAuto) {
		for (short channel = 101 + note.channel; channel < 132; channel ++) {
			short *pt = getNote(channel, note.time);
			if (*pt == 0) {
				*pt = note.keysoundID;
				break;
			}
		}
	} else {
		if (note.length > 1.5 / 192) {
			*(getNote(channelMap[note.channel] + 40, note.time)) = note.keysoundID;
			*(getNote(channelMap[note.channel] + 40, note.time + note.length)) = note.keysoundID;
		} else {
			*(getNote(channelMap[note.channel], note.time)) = note.keysoundID;
		}
	}
}

string base36(int in) {
	static char charmap[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	string ret = "--";
	ret[0] = charmap[in / 36];
	ret[1] = charmap[in % 36];
	return ret;
}

int gcd(int a, int b) {
	if (a < b) {
		return gcd(b, a);
	}
	if (b == 0) {
		return a;
	}
	return gcd(b, a % b);
}

void exportNotes(vector<Note> &foreground, vector<Note> &background) {
	
	string outfilename = outname.substr(0, outname.size() - 4) + "_notes";
	NotesExportFile out;
	
	map<float, float>::iterator
		ub = tempoChanges.end(),
		it = tempoChanges.begin();
	
	printf("Exporting Notes...\n");
	
	vector<float> bpmList;
	for (; it != ub; it ++) {
		bpmList.push_back(it->second);
		*(out.getNote(8, it->first)) = bpmList.size();
	}
	
	out.addNotesFromNotes(foreground, false);
	out.addNotesFromNotes(background, true);
	
	FILE *fo = fopen(outfilename.c_str(), "w");
	
	{
		map<int16_t, Sample *>::iterator sit;
		for (sit = keysounds.begin(); sit != keysounds.end(); sit ++) {
			fprintf(fo, "#WAV%s %s\n", base36(sit->first).c_str(), sit->second->filename.c_str());
		}
	}
	
	map<short, NotesExportMeasure *>::iterator mi;
	map<short, NotesExportChannel *>::iterator ci;
	
	for (mi = out.measures.begin(); mi != out.measures.end(); mi ++) {
		NotesExportMeasure *measure = mi->second;
		for (ci = measure->channels.begin(); ci != measure->channels.end(); ci ++) {
			NotesExportChannel *channel = ci->second;
			NotesExportData *notes = &channel->notes;
			fprintf(fo, "#%03d%02d:", mi->first, ci->first > 100 ? 1 : ci->first);
			{
				int skip = 192;
				for (int i = 0; i < 192; i ++) {
					if ((*notes)[i] != 0) {
						skip = gcd(skip, i);
					}
				}
				for (int i = 0; i < 192; i += skip) {
					if (ci->first > 10) {
						fprintf(fo, "%s", base36((*notes)[i]).c_str());
					} else {
						fprintf(fo, "%02X", (*notes)[i]);
					}
				}
			}
			fprintf(fo, "\n");
		}
		fprintf(fo, "\n");
	}
	
}

//-----------------------------------------------------------------

int main(int argc, char *argv[]) {

	if (argc < 2) {
		printf("Usage: %s <filename.xnt> [<output.wav>]\n", argv[0]);
		return 0;
	}

	inputname = argv[1];
	
	size_t prefixPosition = inputname.find_last_of("\\/");
	
	if (prefixPosition == string::npos) {
		prefixPosition = -1;
	}
	
	dirname = inputname.substr(0, prefixPosition + 1);
	notename = inputname.substr(prefixPosition + 1);
	
	if (notename == "") {
		printf("No filename given.\n");
		return 0;
	}
	
	if (notename[notename.size() - 1] == 'e') {
		notename[notename.size() - 1] = 't';
	}
	
	xnename = notename.substr(0, notename.size() - 1) + "e";
	volname = "volumes.txt";
	
	if (argc < 3) {
		outname = dirname + notename.substr(0, notename.size() - 3) + "wav";
	} else {
		outname = argv[argc - 1];
	}
		
	if (!(fp = fopen((dirname + xnename).c_str(), "rb"))) {
		printf("XNE file not found.\n");
		return 0;
	}
	
	{
		char filechars[257];
		fgets(filechars, 256, fp);
		fclose(fp);
		
		string contents = filechars;
		size_t tempoPosStart, tempoPosEnd;
		
		tempoPosStart = contents.find("Tempo=\"");
		if (tempoPosStart == string::npos) {
			return false;
		}
		tempoPosEnd = contents.find("\"", tempoPosStart + 7);
		string sTempo = contents.substr(tempoPosStart + 7, tempoPosEnd - tempoPosStart - 7);
		
		sscanf(sTempo.c_str(), "%f", &initialTempo);
		if (initialTempo == 0.0f) {
			printf("XNE file has invalid tempo.\n");
			return 0;
		}
	}
	
	map<int16_t, float> volumeMap;
	if ((fp = fopen((dirname + volname).c_str(), "r"))) {
		int first, last;
		float volume;
		while (fscanf(fp, "%d%d%f", &first, &last, &volume) == 3) {
			for (int16_t keysoundID = first; keysoundID <= last; keysoundID ++) {
				volumeMap[keysoundID] = volume;
				printf("volume[%d] = %f\n", keysoundID, volume);
			}
		}
		fclose(fp);
	}
	
	if (!(fp = fopen((dirname + notename).c_str(), "r"))) {
		printf("XNT file not found.\n");
		return 0;
	}
	
	{
		char identifier[4];
		read(identifier);
		
		if (strncmp(identifier, "XNOT", 4) != 0) {
			printf("Not an XNT file.\n");
			return 0;
		}
	}
	
	int32_t numSegments;
	skip(2);
	read(numSegments);
	skip(1);
	
	if (numSegments != 2 && numSegments != 3) {
		printf("XNT file has unknown number of segment.\n");
		return 0;
	}
	
	if (numSegments == 3) {
		readBpmChanges();
	}
	
	vector<Note> keysoundTracks, autoKeysoundTracks;
	readNotes(keysoundTracks);
	readNotes(autoKeysoundTracks);
	readKeysoundDefinition();
	fclose(fp);
	
#if EXPORT_NOTES
	exportNotes(keysoundTracks, autoKeysoundTracks);
#endif	
	
	int minMeasure = INT_MAX;
	updateMinMeasureFromNotes(keysoundTracks,     minMeasure);
	updateMinMeasureFromNotes(autoKeysoundTracks, minMeasure);
	
	float shift = timeToSeconds((float)minMeasure);
	addEventsFromNotes(keysoundTracks,     shift);
	addEventsFromNotes(autoKeysoundTracks, shift);
	sort(soundEvents.begin(), soundEvents.end(), eventcmp);
	
	writeSound(volumeMap);

}

