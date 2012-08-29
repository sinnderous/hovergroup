% configureKayakMPC
% script for setting MPC and kayak model parameters
% used with kayak_MPC_KF AND MOOSkayakMPC
% calls generateTracklinesMPC

% changes
%{
- 8/14 changed system so that + rudder causes + heading (compass bearing),
added Bin
- 8/15/2012 - moved major settings here from generateTracklinesMPC
- 8/17/2012 - added kayak models
- 8/19/2012 - added packet loss params
- 8/19/2012 - added CL heading system
- 8/20/2012 - added option for CL heading to have n=3 or n=4

%}

ifQuiet = 1;          % if cvx is run in quiet mode
uDelay = 1;
plotStep = 0;
% packet loss probability:
probPLoss = .0;
loss2MPC = 0;%%%% currently doesn't work ...

%% PARAMETERS

% System Params

% rudder offset: (for use with MOOS)
%rOff = 3;
rOff=0;
trueNorthAdjustment = -15;

%syss='crossTrack';
syss = 'crossTrack_CLheading';

%kayak = 'kassandra_modem_smallR';
%kayak = 'kassandra_modem_30R';
kayak = 'nostromo_modem';

% Planning horizon (steps)
T = 11;
% T = 6;
% T = 15;

% Time step (sec)
%dt = 1;
dt = 3;
%dt = 6;

% for gen matrices for KF @ 2hz
%dt = 0.5;
%dt = 1/5;

%tracklineType='straight';
tracklineType = 'pavilion_1turn';
%tracklineType = 'hexagon';

switch tracklineType
    case 'straight'
        %Nsec=30;
        Nsec=60;
        ox = 20;oy = -30;
        % bearing of straight line:
        desB = deg2rad(80);
    case 'pavilion_1turn'
        %secPerLeg = ceil(120/dt)*dt;
        secPerLeg = ceil(60/dt)*dt;
        numLegs=2;
        Nsec = secPerLeg*numLegs;
        ox = 0;oy = 150;               
        % pavAngle = 37; pavAngleOffset = -20;
        % startAngle = pavAngle + pavAngleOffset;
        startAngle = 180;     
        kinkAng = 45;  % second leg is after kink, + angle turns right
    case 'hexagon'
        numLegs = 6;
        secPerLeg = 60;
        Nsec = secPerLeg*numLegs;
        ox = 0;
        oy = 200;
end
N = ceil(Nsec/dt);  % total sim steps
r0=0;
hd0=180;


switch syss
    case 'crossTrack'
        n = 4;  % STATES
        m = 1;  % CONTROL INPUT
        q = 2;  % MEASUREMENTS
    case 'crossTrack_CLheading'
        n = 4;  % STATES
        m = 1;  % CONTROL INPUT
        q = 1;  % MEASUREMENTS  
end

% SIM initial state (ACTUAL BEARING): (IN PHYSICAL UNITS)
% [headingAccel(deg/s^2), headingRate(deg/s), heading(deg), crossTrack (m)]
%heading0 = 73+20;
heading0 = 185;
ex0 = 0;
hddmax = 30;
hdmax = 30;
hmax = 90;
exmax = 100;
dhmax = 90;
switch syss
    case 'crossTrack'
        x0c = [0;0;heading0;ex0];
        xmax = [hddmax hdmax hmax exmax]'.*ones(n,1);xmin=-xmax;
        % u is rudder angle
        umax = 20*ones(m,1); umin = -umax;
        slewRate=(10)*dt; % deg per time step
        
        Qmpc = eye(n);
        Pmpc = 10*eye(n);
        Qmpc(n,n) = 2;
        Pmpc(n,n) = 10*2;
        
    case 'crossTrack_CLheading'
        x0c = [0;0;heading0;ex0];
        xmax = [dhmax hdmax hmax exmax]'.*ones(n,1);xmin=-xmax;
        % u is the CHANGE in setpt for desired heading error 
        umax = 90*ones(m,1); umin = -umax;
        slewRate = 45;   % deg per time step
        
        Qmpc = eye(n);
        Pmpc = 10*eye(n);
        Qmpc(1,1) = 0;      % no cost on setpoint
        Qmpc(n,n) = 2;  % cross-track error
        Pmpc(1,1) = 0;
        Pmpc(n,n) = 2*10;
        
end

% MPC params
mu=10;              % sparse control weight

% System params
% kayak cross-track model
switch kayak
    case 'nostromo_modem'
        Krate=1/1.56; % rudder in to heading rate out
        wn=sqrt(1.56);
        zeta=1.01/(2*wn);
        speed=2; % m/s
    case 'kassandra_modem_smallR'
        Krate = 1.37/1.13;
        wn = sqrt(1.13);
        zeta = 0.5/(2*wn);
        speed = 0.8;
    case 'kassandra_modem_30R'
        Krate = 1.19/1.13;
        wn = sqrt(1.13);
        zeta = 1.09/(2*wn);
        speed = 0.8;    % with 65% thrust...?
end

% constrain perpendicular speed to be fraction of speed*dt
% (half: max 30 deg heading diff) Taylor 1st order |error|=0.0236
% (1/3: max 20 deg heading diff) Taylor 1st order |error|=0.007
angle2speed = 1/2;
%angle2speed=1;

% KF params
Bnoise=eye(n);  % process noise input gain

% cross-track:
%Qcross=1e-2;    % continuous time PSD
%Qheading=1e-2;
Qcross = 5;
Qheading = 5;

Qkfc=zeros(n);
if(strcmp(syss,'crossTrack_CLheading'))
    Qkfc(1,1)=0;
end
Qkfc(n-1,n-1)=Qheading;
Qkfc(n,n)=Qcross;
%Qkfc=[0 0 0 0;0 0 0 0;0 0 Qheading 0;0 0 0 Qcross];
Qkfd = Qkfc/dt;
% noise in heading and cross-track (correlation...)?

% measurement noise:
RCompass=.5;       % compass var.
RGPS=3;           % GPS var.
switch syss
    case 'crossTrack'
        Rkf=[RCompass 0;0 RGPS];
    case 'crossTrack_CLheading'
        Rkf=RGPS;
end
% number of 'continuous-time' samples in one time step
nc=dt/(1e-1);

%% compute desired trajectory
% (in generateTracklinesMPC)
generateTracklinesMPC

%% generate A, B matrices

s = tf('s');
K=wn^2*Krate;
headingRate = (K/(s^2 + s + wn^2));
heading=headingRate/s;
% dz/dt = U*sin(theta) ~ U*theta (in radians)
Kcross=speed*pi/180;

switch syss
    case 'crossTrack'
        
        crossTrack = heading*Kcross/s;
        sysC=crossTrack;
        
        % set up system
        [num den] = tfdata(sysC);
        % [num den] = tfdata(sys*exp(-s*sysDelay));
        
        [Ac Bc Cc Dc] = tf2ss(num{1},den{1});
        Cc = [1 0 0 0;0 K 0 0;0 0 1 0;Cc];
        
        % signs and input for desired heading
        Ac(n,n-1) = -1;
        Bin = [0 0 0 0;0 0 0 0;0 0 0 0;0 0 1 0];
        
        sysCss=ss(Ac,Bc,Cc,Dc);
        sysd = c2d(sysCss,dt);
        
        [Ad Bd CdAll Dd] = ssdata(sysd);    % this uses full state output
        Cd = [0,0,CdAll(3,3),0;0,0,0,CdAll(4,4)];   % this for MEASUREMENT
        
        % repeat to get noise input
        sysdNoise = c2d(ss(Ac,Bnoise,Cc,Dc),dt);
        [~,Bdnoise,~,~] = ssdata(sysdNoise);
        
        % repeat to get setpoint input
        sysdSetpt = c2d(ss(Ac,Bin,Cc,Dc),dt);
        [~,Bdin,~,~] = ssdata(sysdSetpt);
        
    case 'crossTrack_CLheading'
        
        wnH = 1;
        zetaH = .7;
        tauRudder = 0.25;
        TFrudder=1;
        % this version just uses stable 2nd order for heading
        CLheading = wnH^2/(s^2+2*wnH*zetaH*s+wnH^2)*TFrudder;
        crossTrack = CLheading*Kcross/s;
        
        %%%%%%%%%%%%%%%%%%%%%
        sysC = crossTrack;
        % set up system
        [num den] = tfdata(sysC);
        
        [Ac3 Bc3 Cc3 Dc] = tf2ss(num{1},den{1});
        % add commanded heading state
        Ac = zeros(n);
        Ac(2:4,1:3) = Ac3;
        Ac(2,4) = 1;
        % Bc is input matrix for dPSI (change in setpt for CL heading)
        Bc = [1 0 0 0]';
        Cc = [1 0 0 0;0 1 0 0;0 0 1 0;0 Cc3];
        
        
        %%%%%%%%%%%% FIX AFTER THIS
        % Bin is input matrix for desired bearing of trackline
        Bin = zeros(n);
        if(n==3)
            Bin(n,:) = [0 1 0];
        elseif(n==4)
            Bin(n,:) = [0 0 1 0];
        end
        
        sysCss=ss(Ac,Bc,Cc,Dc);
        sysd = c2d(sysCss,dt);
        
        [Ad Bd CdAll Dd] = ssdata(sysd);    % this uses full state output
        if(n==3)
            Cd = [0,0,CdAll(n,n)];   % this for MEASUREMENT
        elseif(n==4)
            Cd = [0,0,0,CdAll(n,n)];
        end
        
        % repeat to get noise input
        sysdNoise = c2d(ss(Ac,Bnoise,Cc,Dc),dt);
        [~,Bdnoise,~,~] = ssdata(sysdNoise);
        
        % repeat to get setpoint input
        sysdSetpt = c2d(ss(Ac,Bin,Cc,Dc),dt);
        [~,Bdin,~,~] = ssdata(sysdSetpt);
        
        
end

% convert z, max/min, Qmpc to discrete time states
x0 = CdAll\x0c;
xmax = CdAll\xmax; xmin = CdAll\xmin;
Qmpc = Qmpc.*CdAll;
Pmpc = Pmpc.*CdAll;
Qhalf = sqrtm(Qmpc);

Qkfd = Qkfd/(CdAll.^2);


% setup structures to pass to KF, MPC
sys=struct('n',n,'m',m,'Ad',Ad,'Bd',Bd,'Cd',Cd,'Bdnoise',Bdnoise,...
    'dt',dt,'Bdin',Bdin);
MPCparams=struct('Qhalf',Qhalf,'Pmpc',Pmpc,'mu',mu,'T',T,...
    'ifQuiet',ifQuiet,...
    'speed',speed,'angle2speed',angle2speed,'slewRate',slewRate,...
    'xmax',xmax,'xmin',xmin,'umax',umax,'umin',umin,'CdAll',CdAll);
KFparams=struct('Rkf',Rkf,'Qkfd',Qkfd);

%% plot step responses
if(plotStep)
    tmax=dt*5;stepin=15;  % deg, input
    [y t1] = step(headingRate,tmax);[y,t2] =step(heading,max(t1));
    [y,tc]= step(crossTrack,max(t1));[y_d,td] = step(sysd,max(tc));
    figure
    colorvec={'g--.','b--.','r--.','k--.'};
    for i = 1:n
        stairs(td,stepin*y_d(:,i),colorvec{i})
        xlabel('sec');hold on
    end
    legend('heading accel','heading rate','heading','cross-track')
    figure;subplot(1,2,1)
    step(sysCss*15,tmax)
    hold on;step(sysd*15,tmax);subplot(1,2,2);step(sysd*15,tmax)
end


