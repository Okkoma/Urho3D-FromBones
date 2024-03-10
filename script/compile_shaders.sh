#!/bin/bash

# Parse Arguments
POSITIONAL=()
while [[ $# -gt 0 ]]; do
	key="$1"

	case $key in
		-d|-debug)
		OPTIONPACK="-debug"
		shift
		;;
	
		*)    # unknown option
		POSITIONAL+=("$1") # save it in an array for later
		shift # past argument
		;;
	esac

done

# restore positional parameters
set -- "${POSITIONAL[@]}"

INPUT="$1"
OUTPUT="$2"

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
	PLATFORM=linux	
	# check for WSL 
	WSL=`uname -r | grep Microsoft`
	if [[ "$WSL" != "" ]]; then
		PLATFORM=win64
		WINDOWS=1
		LINUX=0
	fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
	PLATFORM=macosx
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]]; then
	PLATFORM=win64
fi

# Obtient la liste des projets
projectpair=""
if [[ "$PLATFORM" == "linux" ]]; then
    projectnames="/home/chris/dev-projects.txt"
elif [[ "$PLATFORM" == "win64" ]]; then
	MINGW_DRIVE=/mnt/d
	DRIVE="D:"
	projectnames=$MINGW_DRIVE/DEV/Projets/dev-projects.txt
fi

# Vérifier les correspondances dans la liste des projets pour le repertoire courant
while IFS= read -r line; do
    projectname="$(echo "$line" | cut -d ':' -f 1)"
    if [ -n "$(pwd | grep "$projectname")" ]; then
        projectpair="$line"
        break
    fi
done < "$projectnames"

# Quitter si aucun projet n'est trouvé
if [ -z "$projectpair" ]; then
    echo "Aucun projet trouvé pour $(basename "$PWD") dans $projectnames"
    exit 1
else
    URHO3D="$(echo "$projectpair" | cut -d ':' -f 2)"
fi

HOME=`pwd`

if [[ "$PLATFORM" == "linux" ]]; then
    DEVPATH=/media/DATA/Dev/Urho3D
	COMPILER="/media/DATA/Dev/Vulkan/1.2.198.1/x86_64/bin/glslc"
	PACKER="$DEVPATH/$URHO3D/tools/SpirvShaderPacker"
elif [[ "$PLATFORM" == "win64" ]]; then
	DEVPATH=$MINGW_DRIVE/DEV/Urho3D
	COMPILER="/mnt/d/DEV/VulkanSdk/1.3.204.1/Bin/glslc.exe"
	PACKER="$DEVPATH/$URHO3D/tools/SpirvShaderPacker.exe"	
fi

echo "compiler=$COMPILER $OPTIONCOMP"
echo "packer=$PACKER $OPTIONPACK"
echo "input=$INPUT"
echo "ouput=$OUTPUT"

mkdir -p $OUTPUT

cd $INPUT

for ext in vert frag
do
	if [[ $ext == "vert" ]]; then
		ext2=vs.spv
		ext3=vs5
	else	
		ext2=ps.spv
		ext3=ps5
	fi
	
	for file in $(ls *.$ext)
	do
		name="${file%.${ext}}" 
		#echo "  compiling $file to $name.$ext2"
		$COMPILER $OPTIONCOMP -o $name.$ext2 $file
		#echo "  packing $name.$ext2 to $name.$ext3"
		$PACKER $OPTIONPACK $name.$ext2
		echo "  generating $name.$ext3"
	done
done
	
mv *.vs5 $OUTPUT
mv *.ps5 $OUTPUT
rm -f *.spv

cd $HOME

