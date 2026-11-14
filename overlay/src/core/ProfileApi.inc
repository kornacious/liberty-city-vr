#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
// ---------------------------------------------------------------------------
// INI implementation
// ---------------------------------------------------------------------------

namespace {

std::string
profilePath(const char *fileName)
{
	std::string path = fileName != nullptr ? fileName : "";
	// Desktop call sites intentionally use Win32-style ".\\name.ini".
	// Android fopen treats backslash as a literal filename character, so
	// canonicalize separators while keeping the same relative data directory.
	for(char &c : path)
		if(c == '\\')
			c = '/';
	if(!path.empty() && path[0]!='/' && !(path.size()>1 && path[1]==':'))
        path=std::string(platform::gameDataRoot())+"/"+path;
    return std::filesystem::u8path(path).lexically_normal().generic_u8string();
}

char *
trim(char *s)
{
	while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;
	char *end = s + strlen(s);
	while(end > s && (end[-1] == ' ' || end[-1] == '\t' ||
	                  end[-1] == '\r' || end[-1] == '\n'))
		end--;
	*end = '\0';
	return s;
}

bool
readLines(const char *fileName, std::vector<std::string> &lines)
{
	const std::string path = profilePath(fileName);
	FILE *f = fopen(path.c_str(), "rb");
	// Builds before path normalization created a literal Android filename
	// containing the Win32 backslash (".\vr_settings.ini"). Read it once as
	// a fallback; the next profile write migrates the same contents into the
	// canonical "./vr_settings.ini" without discarding user calibration.
	if(f == nullptr && fileName != nullptr && path != fileName)
		f = fopen(fileName, "rb");
	if(f == nullptr)
		return false;
	std::string current;
	int c;
	while((c = fgetc(f)) != EOF){
		if(c == '\n'){
			lines.push_back(current);
			current.clear();
		}else if(c != '\r'){
			current.push_back((char)c);
		}
	}
	if(!current.empty())
		lines.push_back(current);
	fclose(f);
	return true;
}

// Returns true when the line is "[wanted]".
bool
isSectionHeader(const std::string &line, std::string *nameOut)
{
	std::string copy = line;
	char *t = trim(&copy[0]);
	size_t len = strlen(t);
	if(len < 2 || t[0] != '[' || t[len-1] != ']')
		return false;
	if(nameOut != nullptr)
		nameOut->assign(t + 1, len - 2);
	return true;
}

// Splits "key = value"; returns false for blanks and comments.
bool
splitEntry(const std::string &line, std::string *keyOut, std::string *valueOut)
{
	std::string copy = line;
	char *t = trim(&copy[0]);
	if(t[0] == '\0' || t[0] == ';' || t[0] == '#')
		return false;
	char *eq = strchr(t, '=');
	if(eq == nullptr)
		return false;
	*eq = '\0';
	if(keyOut != nullptr)
		keyOut->assign(trim(t));
	if(valueOut != nullptr)
		valueOut->assign(trim(eq + 1));
	return true;
}

bool
lookup(const char *section, const char *key, const char *fileName,
       std::string &valueOut)
{
	std::vector<std::string> lines;
	if(!readLines(fileName, lines))
		return false;

	bool inSection = false;
	std::string name, entryKey, entryValue;
	for(const std::string &line : lines){
		if(isSectionHeader(line, &name)){
			inSection = strcasecmp(name.c_str(), section) == 0;
			continue;
		}
		if(!inSection)
			continue;
		if(!splitEntry(line, &entryKey, &entryValue))
			continue;
		if(strcasecmp(entryKey.c_str(), key) == 0){
			valueOut = entryValue;
			return true;
		}
	}
	return false;
}

} // namespace

extern "C" int32_t
GetPrivateProfileIntA(const char *section, const char *key,
                      int32_t defaultValue, const char *fileName)
{
	std::string value;
	if(!lookup(section, key, fileName, value) || value.empty())
		return defaultValue;
	// Win32 stops at the first non-numeric character and treats a leading
	// minus as valid; strtol matches that closely enough for the settings and
	// calibration values the VR layer stores.
	return (int32_t)strtol(value.c_str(), nullptr, 10);
}

extern "C" uint32_t
GetPrivateProfileStringA(const char *section, const char *key,
                         const char *defaultValue, char *out,
                         uint32_t outSize, const char *fileName)
{
	if(out == nullptr || outSize == 0)
		return 0;

	std::string value;
	if(!lookup(section, key, fileName, value))
		value = defaultValue != nullptr ? defaultValue : "";

	uint32_t copied = (uint32_t)value.size();
	if(copied > outSize - 1)
		copied = outSize - 1;
	memcpy(out, value.c_str(), copied);
	out[copied] = '\0';
	return copied;
}

extern "C" int32_t
WritePrivateProfileStringA(const char *section, const char *key,
                           const char *value, const char *fileName)
{
	std::vector<std::string> lines;
	readLines(fileName, lines);	// absent file is fine, we create it

	std::string name, entryKey;
	int sectionStart = -1;
	int sectionEnd = (int)lines.size();

	for(size_t i = 0; i < lines.size(); i++){
		if(!isSectionHeader(lines[i], &name))
			continue;
		if(sectionStart >= 0){
			sectionEnd = (int)i;
			break;
		}
		if(strcasecmp(name.c_str(), section) == 0)
			sectionStart = (int)i;
	}

	std::string entry = std::string(key) + " = " + (value ? value : "");

	if(sectionStart < 0){
		if(!lines.empty() && !lines.back().empty())
			lines.push_back("");
		lines.push_back(std::string("[") + section + "]");
		lines.push_back(entry);
	}else{
		bool replaced = false;
		for(int i = sectionStart + 1; i < sectionEnd; i++){
			if(!splitEntry(lines[i], &entryKey, nullptr))
				continue;
			if(strcasecmp(entryKey.c_str(), key) == 0){
				lines[i] = entry;
				replaced = true;
				break;
			}
		}
		if(!replaced){
			// Append at the end of the section, before any trailing blanks,
			// so repeated writes do not accumulate empty lines.
			int insertAt = sectionEnd;
			while(insertAt > sectionStart + 1 && lines[insertAt-1].empty())
				insertAt--;
			lines.insert(lines.begin() + insertAt, entry);
		}
	}

	const std::string path = profilePath(fileName);
	static std::atomic<unsigned> sequence{0};
	const std::string temporary=path+".incoming-"+std::to_string(getpid())+"-"+std::to_string(sequence.fetch_add(1));
	const int descriptor=open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0660);
	FILE *f=descriptor>=0?fdopen(descriptor,"wb"):nullptr;
	if(f == nullptr){
		if(descriptor>=0){close(descriptor);unlink(temporary.c_str());}
		LOGE("WritePrivateProfileStringA: cannot open %s: %s", path.c_str(), strerror(errno));
		return 0;
	}
	bool written=true;
	for(const std::string &line : lines)
		if(fprintf(f, "%s\n", line.c_str())<0)written=false;
	if(fclose(f)!=0)written=false;
	if(!written || rename(temporary.c_str(),path.c_str())!=0){
		LOGE("WritePrivateProfileStringA: cannot replace shared settings: %s",strerror(errno));
		unlink(temporary.c_str());return 0;
	}
	return 1;
}
